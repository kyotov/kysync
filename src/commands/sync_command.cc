#include "sync_command.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <src/commands/pb/header.pb.h>
#include <zstd.h>

#include <fstream>
#include <future>
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
    int threads)
    : kParent(parent),
      kDataUri(data_uri),
      kMetadataUri(metadata_uri),
      kSeedUri(seed_uri),
      kOutputPath(output_path),
      kCompressionDiabled(compression_disabled),
      kThreads(threads) {}

void SyncCommand::Impl::ParseHeader(const Reader &metadata_reader) {
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

void SyncCommand::Impl::ReconstructSourceChunk(
    int /*id*/,
    size_t start_offset,
    size_t end_offset) {
  LOG_ASSERT(start_offset % block_ == 0);

  auto smart_buffer = std::make_unique<char[]>(block_);
  auto *buffer = smart_buffer.get();

  auto smart_decompression_buffer =
      std::make_unique<char[]>(max_compressed_size_);
  auto *decompression_buffer = smart_decompression_buffer.get();

  auto seed_reader = Reader::Create(kSeedUri);
  auto data_reader = Reader::Create(kDataUri);

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

  for (auto offset = start_offset; offset < end_offset; offset += block_) {
    auto i = offset / block_;
    auto wcs = weak_checksums_[i];
    auto scs = strong_checksums_[i];
    auto data = analysis_[wcs];

    size_t count;

    if (data.seed_offset != -1ULL && scs == strong_checksums_[data.index]) {
      count = seed_reader->Read(buffer, data.seed_offset, block_);
      reused_bytes_ += count;
    } else {
      if (kCompressionDiabled) {
        count = data_reader->Read(buffer, i * block_, block_);
        downloaded_bytes_ += count;
      } else {
        auto size_to_read = compressed_sizes_[i];
        auto offset_to_read_from = compressed_file_offsets_[i];
        LOG_ASSERT(size_to_read <= max_compressed_size_);
        count = data_reader->Read(
            decompression_buffer,
            offset_to_read_from,
            size_to_read);
        downloaded_bytes_ += count;
        auto const expected_size_after_decompression =
            ZSTD_getFrameContentSize(decompression_buffer, count);
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
        auto decompressed_size =
            ZSTD_decompress(buffer, block_, decompression_buffer, count);
        CHECK(!ZSTD_isError(decompressed_size))
            << ZSTD_getErrorName(decompressed_size);
        LOG_ASSERT(decompressed_size <= block_);
        count = decompressed_size;
        decompressed_bytes_ += count;
      }
    }

    output.write(buffer, count);

    if (kVerify) {
      LOG_ASSERT(wcs == weak_checksums_[data.index]);
      if (count == block_) {
        LOG_ASSERT(wcs == WeakChecksum(buffer, count));
        LOG_ASSERT(scs == StrongChecksum::Compute(buffer, count));
      }
    }

    if (i < block_count_ - 1 || size_ % block_ == 0) {
      CHECK_EQ(count, block_);
    } else {
      CHECK_EQ(count, size_ % block_);
    }

    kParent.AdvanceProgress(count);
  }
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
          threads)) {}

SyncCommand::~SyncCommand() = default;

int SyncCommand::Run() { return impl_->Run(); }

void SyncCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  impl_->Accept(visitor);
}

}  // namespace kysync