#include "sync_command.h"

#include <glog/logging.h>
#include <zstd.h>

#include <array>
#include <fstream>
#include <future>
#include <ios>
#include <map>
#include <utility>

#include "../checksums/strong_checksum_builder.h"
#include "../checksums/weak_checksum.h"
#include "../config.h"
#include "../utilities/parallelize.h"
#include "pb/header_adapter.h"

namespace kysync {

void SyncCommand::ParseHeader(Reader &metadata_reader) {
  static constexpr std::streamsize kMaxHeaderSize = 1024;
  std::vector<uint8_t> buffer(kMaxHeaderSize);
  metadata_reader.Read(buffer.data(), 0, kMaxHeaderSize);

  int version = 0;
  header_size_ =
      HeaderAdapter::ReadHeader(buffer, version, size_, block_, hash_);
  CHECK(version == 2) << "unsupported version" << version;
  block_count_ = (size_ + block_ - 1) / block_;
}

template <typename T>
std::streamsize SyncCommand::ReadIntoContainer(
    Reader &metadata_reader,
    std::streamoff offset,
    std::vector<T> &container) {
  std::streamsize size_to_read =
      block_count_ * sizeof(typename std::vector<T>::value_type);
  container.resize(block_count_);
  std::streamsize size_read =
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
  auto metadata_reader = Reader::Create(metadata_uri_);

  StartNextPhase(metadata_reader->GetSize());
  LOG(INFO) << "reading metadata...";

  ParseHeader(*metadata_reader);
  AdvanceProgress(header_size_);
  auto offset = header_size_;
  offset += ReadIntoContainer(*metadata_reader, offset, weak_checksums_);
  offset += ReadIntoContainer(*metadata_reader, offset, strong_checksums_);
  ReadIntoContainer(*metadata_reader, offset, compressed_sizes_);

  UpdateCompressedOffsetsAndMaxSize();

  for (auto index = 0; index < block_count_; index++) {
    (*set_)[weak_checksums_[index]] = true;
    analysis_[weak_checksums_[index]] = {index, -1};
  }
}

void SyncCommand::AnalyzeSeedChunk(
    int /*id*/,
    std::streamoff start_offset,
    std::streamoff end_offset) {
  auto v_buffer = std::vector<char>(2 * block_);
  auto *buffer = v_buffer.data() + block_;
  memset(buffer, 0, block_);

  auto seed_reader = Reader::Create(seed_uri_);
  auto seed_size = seed_reader->GetSize();

  uint32_t running_wcs = 0;

  std::streamsize warmup = block_ - 1;

  for (std::streamoff seed_offset = start_offset;  //
       seed_offset < end_offset;
       seed_offset += block_)
  {
    auto callback = [&](std::streamoff offset, uint32_t wcs) {
      /* The `set` seems to improve performance. Previously the code was:
       * https://github.com/kyotov/ksync/blob/2d98f83cd1516066416e8319fbfa995e3f49f3dd/commands/SyncCommand.cpp#L128-L132
       */
      if (--warmup < 0 && seed_offset + offset + block_ <= seed_size &&
          (*set_)[wcs]) {
        weak_checksum_matches_++;
        auto &data = analysis_[wcs];

        auto source_digest = strong_checksums_[data.index];
        auto seed_digest = StrongChecksum::Compute(buffer + offset, block_);

        // there was a verification here in previous versions...
        // restore if needed for debugging by running blame on this line.
        if (source_digest == seed_digest) {
          (*set_)[wcs] = false;
          warmup = block_ - 1;
          strong_checksum_matches_++;
          data.seed_offset = seed_offset + offset;
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
    running_wcs = WeakChecksum(buffer, block_, running_wcs, callback);

    AdvanceProgress(block_);
  }
}

void SyncCommand::AnalyzeSeed() {
  auto seed_reader = Reader::Create(seed_uri_);
  auto seed_data_size = seed_reader->GetSize();

  StartNextPhase(seed_data_size);
  LOG(INFO) << "analyzing seed data...";

  Parallelize(
      seed_data_size,
      block_,
      block_,
      threads_,
      // TODO(kyotov): fold this function in here so we would not need the
      // lambda
      [this](auto id, auto beg, auto end) { AnalyzeSeedChunk(id, beg, end); });
}

std::fstream SyncCommand::ChunkReconstructor::GetOutputStream(
    std::streamoff start_offset) {
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
      parent_impl_.output_path_,
      std::ios::binary | std::fstream::out | std::fstream::in);
  output.exceptions(std::fstream::failbit | std::fstream::badbit);
  output.seekp(start_offset);
  return std::move(output);
}

std::streamoff SyncCommand::FindSeedOffset(int block_index) const {
  auto wcs = weak_checksums_[block_index];
  auto scs = strong_checksums_[block_index];
  auto analysis_info = analysis_.find(wcs);
  CHECK(analysis_info != analysis_.end());
  auto analysis = analysis_info->second;
  if (scs == strong_checksums_[analysis.index]) {
    return analysis.seed_offset;
  }
  return -1;
}

std::streamsize SyncCommand::ChunkReconstructor::Decompress(
    std::streamsize compressed_size,
    const void *decompression_buffer,
    void *output_buffer) const {
  auto expected_size_after_decompression =
      ZSTD_getFrameContentSize(decompression_buffer, compressed_size);
  CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_ERROR)
      << " Not compressed by zstd!";
  CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_UNKNOWN)
      << "Original size unknown when decompressing.";
  CHECK(expected_size_after_decompression <= parent_impl_.block_)
      << "Expected decompressed size is greater than block size.";
  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  std::streamsize decompressed_size = ZSTD_decompress(
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
    std::streamsize size_to_write) {
  std::streamsize size_consumed = 0;
  std::streamsize size_written = 0;
  for (auto &retrieval_info : batched_retrieval_infos_) {
    auto batch_member_read_size =
        std::min(size_to_write - size_consumed, retrieval_info.size_to_read);
    output_.seekp(retrieval_info.offset_to_write_to);
    std::streamsize batch_member_write_size = 0;
    if (parent_impl_.compression_disabled_) {
      batch_member_write_size = batch_member_read_size;
    } else {
      LOG_ASSERT(retrieval_info.size_to_read == batch_member_read_size)
          << "Size to read expected to be exact for all blocks including the "
             "last one";
      batch_member_write_size =
          Decompress(  // NOLINT(bugprone-narrowing-conversions)
              batch_member_read_size,
              decompression_buffer_.data() + size_consumed,
              buffer_.data() + size_written);
      parent_impl_.decompressed_bytes_ += batch_member_write_size;
    }
    ValidateAndWrite(
        retrieval_info.block_index,
        buffer_.data() + size_written,
        batch_member_write_size);
    size_consumed += batch_member_read_size;
    size_written += batch_member_write_size;
  }
  LOG_ASSERT(size_consumed == size_to_write)
      << "Unexpected scneario where size_to_write from source was "
      << size_to_write << " but could write " << size_consumed << " instead.";
}

void SyncCommand::ChunkReconstructor::FlushBatch(bool force) {
  auto threshold = force ? 1 : parent_impl_.blocks_per_batch_;
  if (batched_retrieval_infos_.size() < threshold) {
    return;
  }
  auto &read_buffer =
      parent_impl_.compression_disabled_ ? buffer_ : decompression_buffer_;
  auto count = data_reader_->Read(read_buffer.data(), batched_retrieval_infos_);
  WriteRetrievedBatch(count);
  batched_retrieval_infos_.clear();
  parent_impl_.downloaded_bytes_ += count;
}

void SyncCommand::ChunkReconstructor::EnqueueBlockRetrieval(
    int block_index,
    std::streamoff begin_offset) {
  std::streamoff offset_to_write_to = output_.tellp();
  if (parent_impl_.compression_disabled_) {
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
  // NOTE: the cast below is needed on MacOS / xcode 12
  output_.seekp(
      output_.tellp() + static_cast<std::streamoff>(parent_impl_.block_));
}

void SyncCommand::ChunkReconstructor::ValidateAndWrite(
    int block_index,
    const char *buffer,
    std::streamsize count) {
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
    std::streamoff start_offset)
    : parent_impl_(parent_instance) {
  buffer_ =
      std::vector<char>(parent_impl_.block_ * parent_impl_.blocks_per_batch_);
  // FIXME(ashish): why is the decompression buffer so big?
  decompression_buffer_ = std::vector<char>(
      parent_impl_.max_compressed_size_ * parent_impl_.blocks_per_batch_);
  seed_reader_ = Reader::Create(parent_impl_.seed_uri_);
  data_reader_ = Reader::Create(parent_impl_.data_uri_);
  output_ = GetOutputStream(start_offset);
}

void SyncCommand::ChunkReconstructor::ReconstructFromSeed(
    int block_index,
    std::streamoff seed_offset) {
  auto count =
      seed_reader_->Read(buffer_.data(), seed_offset, parent_impl_.block_);
  ValidateAndWrite(block_index, buffer_.data(), count);
  parent_impl_.reused_bytes_ += count;
}

void SyncCommand::ReconstructSourceChunk(
    int /*id*/,
    std::streamoff start_offset,
    std::streamoff end_offset) {
  ChunkReconstructor chunk_reconstructor(*this, start_offset);
  LOG_ASSERT(start_offset % block_ == 0);
  for (auto offset = start_offset; offset < end_offset; offset += block_) {
    auto block_index = static_cast<int>(offset / block_);
    std::streamoff seed_offset = FindSeedOffset(block_index);
    if (seed_offset != -1) {
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
  auto data_reader = Reader::Create(data_uri_);
  auto data_size = size_;

  StartNextPhase(data_size);
  LOG(INFO) << "reconstructing target...";

  std::ofstream output(output_path_, std::ios::binary);
  CHECK(output);
  std::filesystem::resize_file(output_path_, data_size);

  Parallelize(
      data_size,
      block_,
      0,
      threads_,
      [this](auto id, auto beg, auto end) {
        ReconstructSourceChunk(id, beg, end);
      });

  StartNextPhase(data_size);
  LOG(INFO) << "verifying target...";

  static constexpr auto kBufferSize = 1024 * 1024;
  std::vector<char> buffer(kBufferSize);

  StrongChecksumBuilder output_hash;

  std::ifstream read_for_hash_check(output_path_, std::ios::binary);
  while (read_for_hash_check) {
    auto count = read_for_hash_check.read(buffer.data(), kBufferSize).gcount();
    output_hash.Update(buffer.data(), count);
    AdvanceProgress(count);
  }

  CHECK_EQ(hash_, output_hash.Digest().ToString())
      << "mismatch in hash of reconstructed data";

  StartNextPhase(0);
}

SyncCommand::SyncCommand(
    const std::string &data_uri,
    const std::string &metadata_uri,
    std::string seed_uri,
    std::filesystem::path output_path,
    bool compression_disabled,
    int num_blocks_in_batch,
    int threads)
    : data_uri_(
          compression_disabled || data_uri.starts_with("memory://") ||
                  data_uri.ends_with(".pzst")
              ? data_uri
              : data_uri + ".pzst"),
      metadata_uri_(
          !metadata_uri.empty() ? metadata_uri : data_uri + ".kysync"),
      seed_uri_(std::move(seed_uri)),
      output_path_(std::move(output_path)),
      compression_disabled_(compression_disabled),
      blocks_per_batch_(num_blocks_in_batch),
      threads_(threads),
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
