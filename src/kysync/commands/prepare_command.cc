#include <glog/logging.h>
#include <ky/file_stream_provider.h>
#include <ky/metrics/metrics.h>
#include <ky/min.h>
#include <ky/observability/observable.h>
#include <ky/parallelize.h>
#include <kysync/checksums/strong_checksum_builder.h>
#include <kysync/checksums/weak_checksum.h>
#include <kysync/commands/prepare_command.h>
#include <kysync/streams.h>
#include <zstd.h>

#include <cinttypes>
#include <fstream>
#include <utility>

#include "pb/header_adapter.h"

namespace kysync {

namespace fs = std::filesystem;

class KySyncTest;

// NOLINTNEXTLINE(fuchsia-multiple-inheritance,fuchsia-virtual-inheritance)
class PrepareCommandImpl final : virtual public ky::observability::Observable,
                                 public PrepareCommand {
  friend class KySyncTest;
  friend class ChunkPreparer;

  fs::path input_file_path_;
  fs::path output_ksync_file_path_;

  ky::FileStreamProvider output_compressed_file_stream_provider_;

  std::streamsize block_size_;
  std::streamsize max_compressed_block_size_;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<std::streamsize> compressed_sizes_;

  int compression_level_ = 1;
  int threads_;

  ky::metrics::Metric compressed_bytes_{};

  [[nodiscard]] const std::vector<uint32_t> &GetWeakChecksums() const override;
  [[nodiscard]] const std::vector<StrongChecksum> &GetStrongChecksums()
      const override;

public:
  PrepareCommandImpl(
      fs::path input_file_path,
      fs::path output_ksync_file_path,
      fs::path output_compressed_file_path,
      std::streamsize block_size,
      int threads);

  int Run() override;

  void Accept(ky::metrics::MetricVisitor &visitor) override;

  class ChunkPreparer final {
    PrepareCommandImpl &prepare_command_;

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
        PrepareCommandImpl &prepare_command,
        std::streamoff start_offset,
        std::streamoff finish_offset);
  };
};

PrepareCommand::~PrepareCommand() = default;

std::unique_ptr<PrepareCommand> PrepareCommand::Create(
    std::filesystem::path input_file_path,
    std::filesystem::path output_ksync_file_path,
    std::filesystem::path output_compressed_file_path,
    std::streamsize block_size,
    int threads) {
  return std::make_unique<PrepareCommandImpl>(
      std::move(input_file_path),
      std::move(output_ksync_file_path),
      std::move(output_compressed_file_path),
      block_size,
      threads);
}

PrepareCommand::PrepareCommand() = default;

const std::vector<uint32_t> &PrepareCommandImpl::GetWeakChecksums() const {
  return weak_checksums_;
}

const std::vector<StrongChecksum> &PrepareCommandImpl::GetStrongChecksums()
    const {
  return strong_checksums_;
}

void PrepareCommandImpl::ChunkPreparer::Prepare() {
  input_.seekg(start_offset_);

  auto block_size = static_cast<std::streamsize>(buffer_.size());

  auto current_offset = start_offset_;
  auto block_index = static_cast<int>(start_offset_ / block_size);

  while (current_offset < finish_offset_) {
    auto size_to_read = ky::Min(block_size, finish_offset_ - current_offset);
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

void PrepareCommandImpl::ChunkPreparer::CompressBuffer(
    int block_index,
    std::streamsize size) {
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

PrepareCommandImpl::ChunkPreparer::ChunkPreparer(
    PrepareCommandImpl &prepare_command,
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

PrepareCommandImpl::PrepareCommandImpl(
    std::filesystem::path input_file_path,
    std::filesystem::path output_ksync_file_path,
    std::filesystem::path output_compressed_file_path,
    std::streamsize block_size,
    int threads)
    : Observable("prepare"),
      input_file_path_(std::move(input_file_path)),
      output_ksync_file_path_(std::move(output_ksync_file_path)),
      output_compressed_file_stream_provider_(
          std::move(output_compressed_file_path)),
      block_size_(block_size),
      max_compressed_block_size_(
          static_cast<std::streamsize>(ZSTD_compressBound(block_size))),
      threads_(threads) {}

int PrepareCommandImpl::Run() {
  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  std::streamsize data_size = std::filesystem::file_size(input_file_path_);
  StartNextPhase(data_size);

  auto block_count = (data_size + block_size_ - 1) / block_size_;

  weak_checksums_.resize(block_count);
  strong_checksums_.resize(block_count);
  compressed_sizes_.resize(block_count);

  auto compressed_buffer = std::vector<char>(max_compressed_block_size_);

  ky::parallelize::Parallelize(
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

  auto compressed_input =
      output_compressed_file_stream_provider_.CreateFileStream();
  auto compressed_output =
      output_compressed_file_stream_provider_.CreateFileStream();

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

void PrepareCommandImpl::Accept(ky::metrics::MetricVisitor &visitor) {
  VISIT_METRICS(compressed_bytes_);
}

}  // namespace kysync