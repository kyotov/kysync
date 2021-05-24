#include "sync_command.h"

#include <glog/logging.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <src/commands/pb/header.pb.h>
#include <zstd.h>

#include <fstream>
#include <future>
#include <ios>
#include <map>

#include "../checksums/strong_checksum_builder.h"
#include "../checksums/weak_checksum.h"
#include "../config.h"
#include "../utilities/parallelize.h"

namespace kysync {

void SyncCommand::ParseHeader(Reader &metadata_reader) {
  constexpr size_t kMaxHeaderSize = 1024;
  uint8_t buffer[kMaxHeaderSize];

  metadata_reader.Read(buffer, 0, kMaxHeaderSize);
  auto cs = google::protobuf::io::CodedInputStream(buffer, kMaxHeaderSize);
  auto header = Header();
  google::protobuf::util::ParseDelimitedFromCodedStream(&header, &cs, nullptr);

  CHECK(header.version() == 2) << "unsupported version" << header.version();
  size_ = header.size();
  block_ = header.block_size();
  block_count_ = (size_ + block_ - 1) / block_;
  hash_ = header.hash();
  LOG(INFO) << header.DebugString();
  header_size_ = cs.CurrentPosition();
}

template <typename T>
size_t SyncCommand::ReadIntoContainer(
    Reader &metadata_reader,
    size_t offset,
    std::vector<T> &container) {
  size_t size_to_read =
      block_count_ * sizeof(typename std::vector<T>::value_type);
  container.resize(block_count_);
  size_t size_read =
      metadata_reader.Read(container.data(), offset, size_to_read);
  CHECK_EQ(size_to_read, size_read) << "cannot Read metadata";
  AdvanceProgress(size_read);
  return size_read;
}

void SyncCommand::UpdateCompressedOffsetsAndMaxSize() {
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

void SyncCommand::ReadMetadata() {
  auto metadata_reader = Reader::Create(kMetadataUri);

  StartNextPhase(metadata_reader->GetSize());
  LOG(INFO) << "reading metadata...";

  ParseHeader(*metadata_reader);
  AdvanceProgress(header_size_);
  auto offset = header_size_;
  offset += ReadIntoContainer(*metadata_reader, offset, weak_checksums_);
  offset += ReadIntoContainer(*metadata_reader, offset, strong_checksums_);
  offset += ReadIntoContainer(*metadata_reader, offset, compressed_sizes_);

  UpdateCompressedOffsetsAndMaxSize();

  for (size_t index = 0; index < block_count_; index++) {
    (*set_)[weak_checksums_[index]] = true;
    analysis_[weak_checksums_[index]] = {index, -1ULL};
  }
}

void SyncCommand::AnalyzeSeedChunk(
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
          (*set_)[wcs]) {
        weak_checksum_matches_++;
        auto &data = analysis_[wcs];

        auto source_digest = strong_checksums_[data.index];
        auto seed_digest = StrongChecksum::Compute(buffer + offset, block_);

        if (source_digest == seed_digest) {
          (*set_)[wcs] = false;
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

    AdvanceProgress(block_);
  }
}

void SyncCommand::AnalyzeSeed() {
  auto seed_reader = Reader::Create(kSeedUri);
  auto seed_data_size = seed_reader->GetSize();

  StartNextPhase(seed_data_size);
  LOG(INFO) << "analyzing seed data...";

  Parallelize(
      seed_data_size,
      block_,
      block_,
      kThreads,
      // TODO: fold this function in here so we would not need the lambda
      [this](auto id, auto beg, auto end) { AnalyzeSeedChunk(id, beg, end); });
}

std::fstream SyncCommand::ChunkReconstructor::GetOutputStream(
    size_t start_offset) {
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
      parent_impl_.kOutputPath,
      std::ios::binary | std::fstream::out | std::fstream::in);
  output.exceptions(std::fstream::failbit | std::fstream::badbit);
  output.seekp(start_offset);
  return std::move(output);
}

bool SyncCommand::FoundMatchingSeedOffset(
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

size_t SyncCommand::ChunkReconstructor::Decompress(
    size_t compressed_size,
    const void *decompression_buffer,
    void *output_buffer) const {
  auto const expected_size_after_decompression =
      ZSTD_getFrameContentSize(decompression_buffer, compressed_size);
  CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_ERROR)
      << " Not compressed by zstd!";
  CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_UNKNOWN)
      << "Original size unknown when decompressing.";
  CHECK(expected_size_after_decompression <= parent_impl_.block_)
      << "Expected decompressed size is greater than block size.";
  auto decompressed_size = ZSTD_decompress(
      output_buffer,
      parent_impl_.block_,
      decompression_buffer,
      compressed_size);
  CHECK(!ZSTD_isError(decompressed_size))
      << ZSTD_getErrorName(decompressed_size);
  LOG_ASSERT(decompressed_size <= parent_impl_.block_);
  return decompressed_size;
}

void SyncCommand::ChunkReconstructor::WriteRetrievedBatch(
    size_t size_to_write) {
  size_t size_consumed = 0;
  size_t size_written = 0;
  for (auto &retrieval_info : batched_retrieval_infos_) {
    auto batch_member_read_size =
        std::min(size_to_write - size_consumed, retrieval_info.size_to_read);
    output_.seekp(retrieval_info.offset_to_write_to);
    auto batch_member_write_size = 0;
    if (parent_impl_.kCompressionDiabled) {
      batch_member_write_size = batch_member_read_size;
    } else {
      LOG_ASSERT(retrieval_info.size_to_read == batch_member_read_size)
          << "Size to read expected to be exact for all blocks including the "
             "last one";
      batch_member_write_size = Decompress(
          batch_member_read_size,
          decompression_buffer_.get() + size_consumed,
          buffer_.get() + size_written);
      parent_impl_.decompressed_bytes_ += batch_member_write_size;
    }
    ValidateAndWrite(
        retrieval_info.block_index,
        buffer_.get() + size_written,
        batch_member_write_size);
    size_consumed += batch_member_read_size;
    size_written += batch_member_write_size;
  }
  LOG_ASSERT(size_consumed == size_to_write)
      << "Unexpected scneario where size_to_write from source was "
      << size_to_write << " but could write " << size_consumed << " instead.";
}

void SyncCommand::ChunkReconstructor::FlushBatch(bool force) {
  if (!(force ||
        batched_retrieval_infos_.size() >= parent_impl_.kNumBlocksPerRetrieval))
  {
    return;
  }
  if (batched_retrieval_infos_.size() <= 0) {
    return;
  }
  char *read_buffer = parent_impl_.kCompressionDiabled
                          ? buffer_.get()
                          : decompression_buffer_.get();
  auto count = data_reader_->Read(read_buffer, batched_retrieval_infos_);
  WriteRetrievedBatch(count);
  batched_retrieval_infos_.clear();
  parent_impl_.downloaded_bytes_ += count;
}

void SyncCommand::ChunkReconstructor::EnqueueBlockRetrieval(
    size_t block_index,
    size_t begin_offset) {
  size_t offset_to_write_to = output_.tellp();
  if (parent_impl_.kCompressionDiabled) {
    batched_retrieval_infos_.push_back(
        {.block_index = block_index,
         .source_begin_offset = begin_offset,
         .size_to_read = parent_impl_.block_,
         .offset_to_write_to = offset_to_write_to});
  } else {
    batched_retrieval_infos_.push_back(
        {.block_index = block_index,
         .source_begin_offset =
             parent_impl_.compressed_file_offsets_[block_index],
         .size_to_read = parent_impl_.compressed_sizes_[block_index],
         .offset_to_write_to = offset_to_write_to});
  }
  output_.seekp(static_cast<size_t>(output_.tellp()) + parent_impl_.block_);
}

void SyncCommand::ChunkReconstructor::ValidateAndWrite(
    size_t block_index,
    const char *buffer,
    size_t count) {
  if (kVerify) {
    auto wcs = parent_impl_.weak_checksums_[block_index];
    auto scs = parent_impl_.strong_checksums_[block_index];
    auto analysis = parent_impl_.analysis_[wcs];
    LOG_ASSERT(wcs == parent_impl_.weak_checksums_[analysis.index]);
    if (count == parent_impl_.block_) {
      LOG_ASSERT(wcs == WeakChecksum(buffer, count));
      LOG_ASSERT(scs == StrongChecksum::Compute(buffer, count));
    }
  }
  if (block_index < parent_impl_.block_count_ - 1 ||
      parent_impl_.size_ % parent_impl_.block_ == 0)
  {
    CHECK_EQ(count, parent_impl_.block_);
  } else {
    CHECK_EQ(count, parent_impl_.size_ % parent_impl_.block_);
  }
  output_.write(buffer, count);
  parent_impl_.AdvanceProgress(count);
}

SyncCommand::ChunkReconstructor::ChunkReconstructor(
    SyncCommand &parent_instance,
    size_t start_offset,
    size_t end_offset)
    : parent_impl_(parent_instance) {
  buffer_ = std::make_unique<char[]>(
      parent_impl_.block_ * parent_impl_.kNumBlocksPerRetrieval);
  decompression_buffer_ = std::make_unique<char[]>(
      parent_impl_.max_compressed_size_ * parent_impl_.kNumBlocksPerRetrieval);
  seed_reader_ = Reader::Create(parent_impl_.kSeedUri);
  data_reader_ = Reader::Create(parent_impl_.kDataUri);
  output_ = GetOutputStream(start_offset);
}

void SyncCommand::ChunkReconstructor::ReconstructFromSeed(
    size_t block_index,
    size_t seed_offset) {
  auto count =
      seed_reader_->Read(buffer_.get(), seed_offset, parent_impl_.block_);
  ValidateAndWrite(block_index, buffer_.get(), count);
  parent_impl_.reused_bytes_ += count;
}

void SyncCommand::ReconstructSourceChunk(
    int /*id*/,
    size_t start_offset,
    size_t end_offset) {
  ChunkReconstructor chunk_reconstructor(*this, start_offset, end_offset);
  LOG_ASSERT(start_offset % block_ == 0);
  for (auto offset = start_offset; offset < end_offset; offset += block_) {
    auto block_index = offset / block_;
    size_t seed_offset = -1ULL;
    if (FoundMatchingSeedOffset(block_index, &seed_offset)) {
      chunk_reconstructor.ReconstructFromSeed(block_index, seed_offset);
    } else {
      chunk_reconstructor.EnqueueBlockRetrieval(block_index, offset);
      chunk_reconstructor.FlushBatch(false);
    }
  }
  // Retrieve trailing batch if any
  chunk_reconstructor.FlushBatch(true);
}

void SyncCommand::ReconstructSource() {
  auto data_reader = Reader::Create(kDataUri);
  auto data_size = size_;

  StartNextPhase(data_size);
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

  StartNextPhase(data_size);
  LOG(INFO) << "verifying target...";

  constexpr auto kBufferSize = 1024 * 1024;
  auto smart_buffer = std::make_unique<char[]>(kBufferSize);
  auto *buffer = smart_buffer.get();

  StrongChecksumBuilder output_hash;

  std::ifstream read_for_hash_check(kOutputPath, std::ios::binary);
  while (read_for_hash_check) {
    auto count = read_for_hash_check.read(buffer, kBufferSize).gcount();
    output_hash.Update(buffer, count);
    AdvanceProgress(count);
  }

  CHECK_EQ(hash_, output_hash.Digest().ToString())
      << "mismatch in hash of reconstructed data";

  StartNextPhase(0);
}

SyncCommand::SyncCommand(
    const std::string &data_uri,
    const std::string &metadata_uri,
    const std::string &seed_uri,
    const std::filesystem::path &output_path,
    bool compression_disabled,
    int num_blocks_in_batch,
    int threads)
    : kDataUri(
          compression_disabled || data_uri.starts_with("memory://") ||
                  data_uri.ends_with(".pzst")
              ? data_uri
              : data_uri + ".pzst"),
      kMetadataUri(!metadata_uri.empty() ? metadata_uri : data_uri + ".kysync"),
      kSeedUri(seed_uri),
      kOutputPath(output_path),
      kCompressionDiabled(compression_disabled),
      kNumBlocksPerRetrieval(num_blocks_in_batch),
      kThreads(threads),
      set_(std::make_unique<std::bitset<k4Gb>>()) {}

int SyncCommand::Run() {
  ReadMetadata();
  AnalyzeSeed();
  ReconstructSource();
  return 0;
}

void SyncCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  VISIT_METRICS(weak_checksum_matches_);
  VISIT_METRICS(weak_checksum_false_positive_);
  VISIT_METRICS(strong_checksum_matches_);
  VISIT_METRICS(reused_bytes_);
  VISIT_METRICS(downloaded_bytes_);
  VISIT_METRICS(decompressed_bytes_);
}

}  // namespace kysync
