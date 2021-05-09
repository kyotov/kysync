#include "sync_command.h"

#include <zstd.h>

#include <fstream>
#include <future>
#include <ios>
#include <map>

#include "../checksums/strong_checksum_builder.h"
#include "../checksums/weak_checksum.h"
#include "../config.h"
#include "../utilities/parallelize.h"
#include "glog/logging.h"
#include "impl/sync_command_impl.h"

namespace kysync {

SyncCommand::Impl::Impl(
    const SyncCommand &parent,
    const std::string &data_uri,
    const std::string &metadata_uri,
    const std::string &seed_uri,
    const std::filesystem::path &output_path,
    bool compression_disabled,
    int num_blocks_in_batch,
    int threads)
    : kParent(parent),
      kDataUri(data_uri),
      kMetadataUri(metadata_uri),
      kSeedUri(seed_uri),
      kOutputPath(output_path),
      kCompressionDiabled(compression_disabled),
      kNumBlocksPerRetrieval(num_blocks_in_batch),
      kThreads(threads) {}

void SyncCommand::Impl::ParseHeader(const Reader &metadata_reader) {
  constexpr size_t kMaxHeaderSize = 1024;
  char buffer[kMaxHeaderSize];

  metadata_reader.Read(buffer, 0, kMaxHeaderSize);

  std::stringstream header;
  header.write(buffer, kMaxHeaderSize);
  header.seekg(0);

  std::map<std::string, std::string> metadata;

  std::string key;
  std::string value;

  while (true) {
    header >> key >> value;
    if (key == "version:") {
      CHECK(value == "1") << "unsupported version " << value;
    } else if (key == "size:") {
      size_ = strtoull(value.c_str(), nullptr, 10);
    } else if (key == "block:") {
      block_ = strtoull(value.c_str(), nullptr, 10);
      block_count_ = (size_ + block_ - 1) / block_;
    } else if (key == "hash:") {
      hash_ = value;
    } else if (key == "eof:") {
      CHECK(value == "1") << "bad eof marker (1)";
      break;
    } else {
      CHECK(false) << "invalid header key `" << key << "`";
    }
  }

  header.read(buffer, 1);
  CHECK(*buffer == '\n') << "bad eof marker (\\n)";

  header_size_ = header.tellg();
}

template <typename T>
size_t SyncCommand::Impl::ReadIntoContainer(
    const Reader &metadata_reader,
    size_t offset,
    std::vector<T> &container) {
  size_t size_to_read =
      block_count_ * sizeof(typename std::vector<T>::value_type);
  container.resize(block_count_);
  size_t size_read =
      metadata_reader.Read(container.data(), offset, size_to_read);
  CHECK_EQ(size_to_read, size_read) << "cannot Read metadata";
  kParent.AdvanceProgress(size_read);
  return size_read;
}

void SyncCommand::Impl::UpdateCompressedOffsetsAndMaxSize() {
  compressed_file_offsets_.push_back(0);
  if (!compressed_sizes_.empty()) {
    max_compressed_size_ = compressed_sizes_[0];
  }
  for (int i = 1; i < block_count_; i++) {
    compressed_file_offsets_.push_back(
        compressed_file_offsets_[i - 1] + compressed_sizes_[i - 1]);
    max_compressed_size_ = std::max(max_compressed_size_, compressed_sizes_[i]);
  }
}

void SyncCommand::Impl::ReadMetadata() {
  auto metadata_reader = Reader::Create(kMetadataUri);

  kParent.StartNextPhase(metadata_reader->GetSize());
  LOG(INFO) << "reading metadata...";

  ParseHeader(*metadata_reader);
  kParent.AdvanceProgress(header_size_);
  auto offset = header_size_;
  offset += ReadIntoContainer(*metadata_reader, offset, weak_checksums_);
  offset += ReadIntoContainer(*metadata_reader, offset, strong_checksums_);
  offset += ReadIntoContainer(*metadata_reader, offset, compressed_sizes_);

  UpdateCompressedOffsetsAndMaxSize();

  for (size_t index = 0; index < block_count_; index++) {
    set_[weak_checksums_[index]] = true;
    analysis_[weak_checksums_[index]] = {index, -1ULL};
  }
}

void SyncCommand::Impl::AnalyzeSeedChunk(
    int /*id*/,
    size_t start_offset,
    size_t end_offset) {
  auto smart_buffer = std::make_unique<uint8_t[]>(2 * block_);
  auto *buffer = smart_buffer.get() + block_;
  memset(buffer, 0, block_);

  auto seed_reader = Reader::Create(kSeedUri);
  auto seed_size = seed_reader->GetSize();

  uint32_t _wcs = 0;

  int64_t warmup = block_ - 1;

  for (size_t seed_offset = start_offset;  //
       seed_offset < end_offset;
       seed_offset += block_)
  {
    auto callback = [&](size_t offset, uint32_t wcs) {
      /* The `set` seems to improve performance. Previously the code was:
       * https://github.com/kyotov/ksync/blob/2d98f83cd1516066416e8319fbfa995e3f49f3dd/commands/SyncCommand.cpp#L128-L132
       */
      if (--warmup < 0 && seed_offset + offset + block_ <= seed_size &&
          set_[wcs]) {
        weak_checksum_matches_++;
        auto &data = analysis_[wcs];

        auto source_digest = strong_checksums_[data.index];
        auto seed_digest = StrongChecksum::Compute(buffer + offset, block_);

        if (source_digest == seed_digest) {
          set_[wcs] = false;
          if (kWarmupAfterMatch) {
            warmup = block_ - 1;
          }
          strong_checksum_matches_++;
          data.seed_offset = seed_offset + offset;

          if (kVerify) {
            auto t = std::make_unique<char[]>(block_);
            seed_reader->Read(t.get(), data.seed_offset, block_);
            LOG_ASSERT(wcs == WeakChecksum(t.get(), block_));
            LOG_ASSERT(seed_digest == StrongChecksum::Compute(t.get(), block_));
          }
        } else {
          weak_checksum_false_positive_++;
        }
      }
    };

    memcpy(buffer - block_, buffer, block_);
    auto count = seed_reader->Read(buffer, seed_offset, block_);
    memset(buffer + count, 0, block_ - count);

    /* I tried to "optimize" the following by manually inlining `weakChecksum`
     * and then unrolling the inner loop. To my surprise this did not help...
     * The MSVC compiler must be already doing it...
     * We should verify that other compilers do reasonably well before we
     * abandon the idea completely.
     * https://github.com/kyotov/ksync/blob/2d98f83cd1516066416e8319fbfa995e3f49f3dd/commands/SyncCommand.cpp#L166-L220
     */
    _wcs =
        WeakChecksum(static_cast<const void *>(buffer), block_, _wcs, callback);

    kParent.AdvanceProgress(block_);
  }
}

void SyncCommand::Impl::AnalyzeSeed() {
  auto seed_reader = Reader::Create(kSeedUri);
  auto seed_data_size = seed_reader->GetSize();

  kParent.StartNextPhase(seed_data_size);
  LOG(INFO) << "analyzing seed data...";

  Parallelize(
      seed_data_size,
      block_,
      block_,
      kThreads,
      // TODO: fold this function in here so we would not need the lambda
      [this](auto id, auto beg, auto end) { AnalyzeSeedChunk(id, beg, end); });
}

std::fstream SyncCommand::Impl::GetOutputStream(size_t start_offset) {
  // NOTE: The fstream object is consciously initialized with both out and in
  // modes although this function only writes to the file.
  // Calling
  //   std::ofstream output(outputPath, std::ios::binary)
  // or calling
  //   std::fstream output(outputPath, std::ios::binary | std::ios::out)
  // causes the file to be truncated which can lead to race conditions if called
  // in each thread. More information about the truncate behavior is available
  // here:
  // https://stackoverflow.com/questions/39256916/does-stdofstream-truncate-or-append-by-default#:~:text=It%20truncates%20by%20default
  // Using ate or app does not resolve this.
  std::fstream output(
      kOutputPath,
      std::ios::binary | std::fstream::out | std::fstream::in);
  output.exceptions(std::fstream::failbit | std::fstream::badbit);
  output.seekp(start_offset);
  return std::move(output);
}

bool SyncCommand::Impl::FoundMatchingSeedOffset(
    size_t block_index,
    size_t *matching_seed_offset) const {
  auto wcs = weak_checksums_[block_index];
  auto scs = strong_checksums_[block_index];
  auto analysis_info = analysis_.find(wcs);
  CHECK(analysis_info != analysis_.end());
  auto analysis = analysis_info->second;
  if (analysis.seed_offset != -1ULL && scs == strong_checksums_[analysis.index])
  {
    *matching_seed_offset = analysis.seed_offset;
    return true;
  } else {
    return false;
  }
}

size_t SyncCommand::Impl::Decompress(
    size_t block_index,
    size_t compressed_size,
    const void *decompression_buffer,
    void *output_buffer) const {
  auto offset_to_read_from = compressed_file_offsets_[block_index];
  auto const expected_size_after_decompression =
      ZSTD_getFrameContentSize(decompression_buffer, compressed_size);
  CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_ERROR)
      << "Offset starting " << offset_to_read_from
      << " not compressed by zstd!";
  CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_UNKNOWN)
      << "Original GetSize unknown when decompressing from offset "
      << offset_to_read_from;
  CHECK(expected_size_after_decompression <= block_)
      << "Expected decompressed GetSize is greater than block GetSize. "
         "Starting offset "
      << offset_to_read_from;
  auto decompressed_size = ZSTD_decompress(
      output_buffer,
      block_,
      decompression_buffer,
      compressed_size);
  CHECK(!ZSTD_isError(decompressed_size))
      << ZSTD_getErrorName(decompressed_size);
  LOG_ASSERT(decompressed_size <= block_);
  return decompressed_size;
}

void SyncCommand::Impl::WriteRetrievedBatch(
    char *buffer,
    const char *decompression_buffer,
    size_t size_to_write,
    std::vector<BatchedRetrivalInfo> &batched_retrievals_info,
    std::fstream &output) {
  size_t size_consumed = 0;
  size_t size_written = 0;
  for (auto &retrieval_batch : batched_retrievals_info) {
    auto batch_member_read_size =
        std::min(size_to_write - size_consumed, retrieval_batch.size_to_read);
    output.seekp(retrieval_batch.offset_to_write_to);
    auto batch_member_write_size = 0;
    if (kCompressionDiabled) {
      batch_member_write_size = batch_member_read_size;
    } else {
      LOG_ASSERT(retrieval_batch.size_to_read == batch_member_read_size)
          << "Size to read expected to be exact for all blocks including the "
             "last one";
      batch_member_write_size = Decompress(
          retrieval_batch.block_index,
          batch_member_read_size,
          decompression_buffer + size_consumed,
          buffer + size_written);
      decompressed_bytes_ += batch_member_write_size;
    }
    ValidateAndWrite(
        retrieval_batch.block_index,
        buffer + size_written,
        batch_member_write_size,
        output);
    size_consumed += batch_member_read_size;
    size_written += batch_member_write_size;
  }
  LOG_ASSERT(size_consumed == size_to_write)
      << "Unexpected scneario where size_to_write from source was "
      << size_to_write << " but could wrote " << size_consumed << " instead.";
}

size_t SyncCommand::Impl::PerformBatchRetrieval(
    const Reader *data_reader,
    char *buffer,
    char *decompression_buffer,
    std::vector<BatchedRetrivalInfo> &batched_retrievals_info,
    std::fstream &output) {
  if (batched_retrievals_info.size() <= 0) {
    return 0;
  }
  char *read_buffer = kCompressionDiabled ? buffer : decompression_buffer;
  auto count = data_reader->Read(read_buffer, batched_retrievals_info);
  WriteRetrievedBatch(
      buffer,
      decompression_buffer,
      count,
      batched_retrievals_info,
      output);
  batched_retrievals_info.clear();
  downloaded_bytes_ += count;
  return count;
}

void SyncCommand::Impl::AddForBatchedRetrieval(
    size_t block_index,
    size_t begin_offset,
    size_t offset_to_write_to,
    std::vector<BatchedRetrivalInfo> &batched_retrievals_info) const {
  if (kCompressionDiabled) {
    batched_retrievals_info.push_back(
        {.block_index = block_index,
         .source_begin_offset = begin_offset,
         .size_to_read = block_,
         .offset_to_write_to = offset_to_write_to});
  } else {
    batched_retrievals_info.push_back(
        {.block_index = block_index,
         .source_begin_offset = compressed_file_offsets_[block_index],
         .size_to_read = compressed_sizes_[block_index],
         .offset_to_write_to = offset_to_write_to});
  }
}

void SyncCommand::Impl::RetrieveFromSource(
    size_t block_index,
    const Reader *data_reader,
    char *decompression_buffer,
    char *read_buffer,
    std::vector<BatchedRetrivalInfo> &batched_retrievals_info,
    std::fstream &output) {
  auto begin_offset = block_index * block_;
  size_t current_write_offset = output.tellp();
  AddForBatchedRetrieval(
      block_index,
      begin_offset,
      current_write_offset,
      batched_retrievals_info);
  if (batched_retrievals_info.size() >= kNumBlocksPerRetrieval) {
    PerformBatchRetrieval(
        data_reader,
        read_buffer,
        decompression_buffer,
        batched_retrievals_info,
        output);
  } else {
    output.seekp(current_write_offset + block_);
  }
}

void SyncCommand::Impl::ValidateAndWrite(
    size_t block_index,
    const char *buffer,
    size_t count,
    std::fstream &output) {
  if (kVerify) {
    auto wcs = weak_checksums_[block_index];
    auto scs = strong_checksums_[block_index];
    auto analysis = analysis_[wcs];
    LOG_ASSERT(wcs == weak_checksums_[analysis.index]);
    if (count == block_) {
      LOG_ASSERT(wcs == WeakChecksum(buffer, count));
      LOG_ASSERT(scs == StrongChecksum::Compute(buffer, count));
    }
  }
  if (block_index < block_count_ - 1 || size_ % block_ == 0) {
    CHECK_EQ(count, block_);
  } else {
    CHECK_EQ(count, size_ % block_);
  }
  output.write(buffer, count);
  kParent.AdvanceProgress(count);
}

void SyncCommand::Impl::ReconstructSourceChunk(
    int /*id*/,
    size_t start_offset,
    size_t end_offset) {
  LOG_ASSERT(start_offset % block_ == 0);
  auto read_buffer = std::make_unique<char[]>(block_ * kNumBlocksPerRetrieval);
  auto decompression_buffer =
      std::make_unique<char[]>(max_compressed_size_ * kNumBlocksPerRetrieval);
  auto seed_reader = Reader::Create(kSeedUri);
  auto data_reader = Reader::Create(kDataUri);
  std::fstream output = GetOutputStream(start_offset);
  std::vector<BatchedRetrivalInfo> batched_retrievals_info;
  size_t count;
  for (auto offset = start_offset; offset < end_offset; offset += block_) {
    auto block_index = offset / block_;
    size_t seed_offset = -1ULL;
    if (FoundMatchingSeedOffset(block_index, &seed_offset)) {
      count = seed_reader->Read(read_buffer.get(), seed_offset, block_);
      reused_bytes_ += count;
      ValidateAndWrite(block_index, read_buffer.get(), count, output);
    } else {
      RetrieveFromSource(
          block_index,
          data_reader.get(),
          decompression_buffer.get(),
          read_buffer.get(),
          batched_retrievals_info,
          output);
    }
  }
  // Retrieve trailing batch if any
  PerformBatchRetrieval(
      data_reader.get(),
      read_buffer.get(),
      decompression_buffer.get(),
      batched_retrievals_info,
      output);
}

void SyncCommand::Impl::ReconstructSource() {
  auto data_reader = Reader::Create(kDataUri);
  auto data_size = size_;

  kParent.StartNextPhase(data_size);
  LOG(INFO) << "reconstructing target...";

  std::ofstream output(kOutputPath, std::ios::binary);
  CHECK(output);
  std::filesystem::resize_file(kOutputPath, data_size);

  Parallelize(
      data_size,
      block_,
      0,
      kThreads,
      [this](auto id, auto beg, auto end) {
        ReconstructSourceChunk(id, beg, end);
      });

  kParent.StartNextPhase(data_size);
  LOG(INFO) << "verifying target...";

  constexpr auto kBufferSize = 1024 * 1024;
  auto smart_buffer = std::make_unique<char[]>(kBufferSize);
  auto *buffer = smart_buffer.get();

  StrongChecksumBuilder output_hash;

  std::ifstream read_for_hash_check(kOutputPath, std::ios::binary);
  while (read_for_hash_check) {
    auto count = read_for_hash_check.read(buffer, kBufferSize).gcount();
    output_hash.Update(buffer, count);
    kParent.AdvanceProgress(count);
  }

  CHECK_EQ(hash_, output_hash.Digest().ToString())
      << "mismatch in hash of reconstructed data";

  kParent.StartNextPhase(0);
}

int SyncCommand::Impl::Run() {
  ReadMetadata();
  AnalyzeSeed();
  ReconstructSource();
  return 0;
}

void SyncCommand::Impl::Accept(MetricVisitor &visitor) {
  VISIT_METRICS(weak_checksum_matches_);
  VISIT_METRICS(weak_checksum_false_positive_);
  VISIT_METRICS(strong_checksum_matches_);
  VISIT_METRICS(reused_bytes_);
  VISIT_METRICS(downloaded_bytes_);
  VISIT_METRICS(decompressed_bytes_);
}

SyncCommand::SyncCommand(
    const std::string &data_uri,
    const std::string &metadata_uri,
    const std::string &seed_uri,
    const std::filesystem::path &output_path,
    bool compression_disabled,
    int num_blocks_in_batch,
    int threads)
    : impl_(new Impl(
          *this,
          compression_disabled || data_uri.starts_with("memory://") ||
                  data_uri.ends_with(".pzst")
              ? data_uri
              : data_uri + ".pzst",
          !metadata_uri.empty() ? metadata_uri : data_uri + ".kysync",
          seed_uri,
          output_path,
          compression_disabled,
          num_blocks_in_batch,
          threads)) {}

SyncCommand::~SyncCommand() = default;

int SyncCommand::Run() { return impl_->Run(); }

void SyncCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  impl_->Accept(visitor);
}

}  // namespace kysync