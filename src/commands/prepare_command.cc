#include "prepare_command.h"

#include <glog/logging.h>
#include <zstd.h>

#include <cinttypes>
#include <fstream>
#include <utility>
#include <vector>

#include "../checksums/strong_checksum_builder.h"
#include "../checksums/weak_checksum.h"
#include "../config.h"
#include "../utilities/file_stream_provider.h"
#include "../utilities/parallelize.h"
#include "../utilities/streams.h"
#include "pb/header_adapter.h"

namespace kysync {

class ChunkPreparer final {
  PrepareCommand &prepare_command_;

  std::ifstream input_;
  std::fstream output_;

  std::streamoff start_offset_;
  std::streamoff finish_offset_;

  std::vector<char> buffer_;
  std::vector<char> compressed_buffer_;

  void Prepare();
  void CompressBuffer(int block_index, std::streamsize size);

public:
  ChunkPreparer(
      PrepareCommand &prepare_command,
      std::streamoff start_offset,
      std::streamoff finish_offset);
};

void ChunkPreparer::Prepare() {
  input_.seekg(start_offset_);

  auto block_size = static_cast<std::streamsize>(buffer_.size());

  auto current_offset = start_offset_;
  auto block_index = static_cast<int>(start_offset_ / block_size);

  while (current_offset < finish_offset_) {
    auto size_to_read = Min(block_size, finish_offset_ - current_offset);
    memset(buffer_.data() + size_to_read, 0, buffer_.size() - size_to_read);

    input_.read(buffer_.data(), size_to_read);
    CHECK(input_);
    CHECK(input_.gcount() == size_to_read);

    // FIXME(kyotov): should this be `size_to_read` instead of `block_size`
    prepare_command_.weak_checksums_[block_index] =
        WeakChecksum(buffer_.data(), block_size);

    prepare_command_.strong_checksums_[block_index] =
        StrongChecksum::Compute(buffer_.data(), block_size);

    CompressBuffer(block_index, size_to_read);

    prepare_command_.AdvanceProgress(size_to_read);

    current_offset += block_size;
    block_index++;
  }
}

void ChunkPreparer::CompressBuffer(int block_index, std::streamsize size) {
  std::streamsize compressed_size =
      ZSTD_compress(  // NOLINT(bugprone-narrowing-conversions,
                      // cppcoreguidelines-narrowing-conversions)
          compressed_buffer_.data(),
          prepare_command_.max_compressed_block_size_,
          buffer_.data(),
          size,
          prepare_command_.compression_level_);
  CHECK(!ZSTD_isError(compressed_size)) << ZSTD_getErrorName(compressed_size);

  output_.seekp(block_index * prepare_command_.max_compressed_block_size_);
  output_.write(compressed_buffer_.data(), compressed_size);
  CHECK(output_);

  prepare_command_.compressed_sizes_[block_index] = compressed_size;
  prepare_command_.compressed_bytes_ += compressed_size;
}

ChunkPreparer::ChunkPreparer(
    PrepareCommand &prepare_command,
    std::streamoff start_offset,
    std::streamoff finish_offset)
    : prepare_command_(prepare_command),
      input_(prepare_command.input_file_path_, std::ios::binary),
      output_(std::move(prepare_command.output_compressed_file_stream_provider_
                            .CreateFileStream())),
      start_offset_(start_offset),
      finish_offset_(finish_offset),
      buffer_(prepare_command.block_size_),
      compressed_buffer_(prepare_command.max_compressed_block_size_) {
  CHECK(start_offset_ % prepare_command_.block_size_ == 0);
  CHECK(input_) << "error reading from " << prepare_command_.input_file_path_;

  Prepare();
}

PrepareCommand::PrepareCommand(
    std::filesystem::path input_file_path,
    std::filesystem::path output_ksync_file_path,
    std::filesystem::path output_compressed_file_path,
    std::streamsize block_size,
    int threads)
    : input_file_path_(std::move(input_file_path)),
      output_ksync_file_path_(std::move(output_ksync_file_path)),
      output_compressed_file_stream_provider_(
          std::move(output_compressed_file_path)),
      block_size_(block_size),
      max_compressed_block_size_(
          static_cast<std::streamsize>(ZSTD_compressBound(block_size))),
      threads_(threads) {}

int PrepareCommand::Run() {
  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  std::streamsize data_size = std::filesystem::file_size(input_file_path_);
  StartNextPhase(data_size);

  auto block_count = (data_size + block_size_ - 1) / block_size_;

  weak_checksums_.resize(block_count);
  strong_checksums_.resize(block_count);
  compressed_sizes_.resize(block_count);

  auto compressed_buffer = std::vector<char>(max_compressed_block_size_);

  Parallelize(
      data_size,
      block_size_,
      0,
      threads_,
      [this](int, auto start_offset, auto finish_offset) {
        auto p = ChunkPreparer(*this, start_offset, finish_offset);
      });

  StrongChecksumBuilder hash;

  auto input = std::ifstream(input_file_path_, std::ios::binary);
  auto buffer = std::vector<char>(block_size_);

  // TODO(kyotov): this is going to improve with issue
  //  https://github.com/kyotov/ksync/issues/100
  StartNextPhase(data_size + 2 * compressed_bytes_);

  auto compressed_input = output_compressed_file_stream_provider_.CreateFileStream();
  auto compressed_output = output_compressed_file_stream_provider_.CreateFileStream();

  for (int i = 0; i < compressed_sizes_.size(); i++) {
    compressed_input.seekg(i * max_compressed_block_size_);
    compressed_input.read(compressed_buffer.data(), compressed_sizes_[i]);
    CHECK(compressed_input);
    CHECK(compressed_input.gcount() == compressed_sizes_[i]);
    compressed_output.write(compressed_buffer.data(), compressed_sizes_[i]);
    CHECK(compressed_output);

    input.read(buffer.data(), block_size_);
    hash.Update(buffer.data(), input.gcount());

    AdvanceProgress(input.gcount() + 2 * compressed_sizes_[i]);
  }

  output_compressed_file_stream_provider_.Resize(compressed_output.tellp());

  // produce the ksync metadata output

  auto output_ksync = std::ofstream(output_ksync_file_path_, std::ios::binary);
  CHECK(output_ksync) << "unable to write to " << output_ksync_file_path_;

  StartNextPhase(1);

  auto header_size = HeaderAdapter::WriteHeader(
      output_ksync,
      2,
      data_size,
      block_size_,
      hash.Digest().ToString());
  AdvanceProgress(header_size);

  AdvanceProgress(StreamWrite(output_ksync, weak_checksums_));
  AdvanceProgress(StreamWrite(output_ksync, strong_checksums_));
  AdvanceProgress(StreamWrite(output_ksync, compressed_sizes_));

  StartNextPhase(0);
  return 0;
}

void PrepareCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  VISIT_METRICS(compressed_bytes_);
}

}  // namespace kysync