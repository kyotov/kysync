#include "prepare_command.h"

#include <glog/logging.h>
#include <zstd.h>

#include <cinttypes>
#include <fstream>
#include <utility>

#include "../checksums/strong_checksum_builder.h"
#include "../checksums/weak_checksum.h"
#include "../utilities/streams.h"
#include "pb/header_adapter.h"

namespace kysync {

PrepareCommand::PrepareCommand(
    fs::path input_file_path,
    fs::path output_ksync_file_path,
    fs::path output_compressed_file_path,
    std::streamsize block_size)
    : input_file_path_(std::move(input_file_path)),
      output_ksync_file_path_(std::move(output_ksync_file_path)),
      output_compressed_file_path_(std::move(output_compressed_file_path)),
      block_size_(block_size)  //
{
  if (output_ksync_file_path_.empty()) {
    output_ksync_file_path_ = input_file_path_;
    output_ksync_file_path_.append(".kysync");
  }
  if (output_compressed_file_path_.empty()) {
    output_compressed_file_path_ = input_file_path_;
    output_compressed_file_path_.append(".pzst");
  }
}

int PrepareCommand::Run() {
  auto v_buffer = std::vector<char>(block_size_);
  auto *buffer = v_buffer.data();

  auto compression_buffer_size = ZSTD_compressBound(block_size_);
  auto compression_buffer = std::vector<char>(compression_buffer_size);

  auto input = std::ifstream(input_file_path_, std::ios::binary);
  CHECK(input) << "error reading from " << input_file_path_;

  auto output = std::ofstream(output_compressed_file_path_, std::ios::binary);
  CHECK(output) << "unable to write to " << output_compressed_file_path_;

  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  std::streamsize data_size = fs::file_size(input_file_path_);
  StartNextPhase(data_size);

  StrongChecksumBuilder hash;

  while (auto count = input.read(buffer, block_size_).gcount()) {
    memset(buffer + count, 0, block_size_ - count);

    hash.Update(buffer, count);

    weak_checksums_.push_back(WeakChecksum(buffer, block_size_));
    strong_checksums_.push_back(StrongChecksum::Compute(buffer, block_size_));

    std::streamsize compressed_size =
        ZSTD_compress(  // NOLINT(bugprone-narrowing-conversions,
                        // cppcoreguidelines-narrowing-conversions)
            compression_buffer.data(),
            compression_buffer_size,
            buffer,
            count,
            compression_level_);
    CHECK(!ZSTD_isError(compressed_size)) << ZSTD_getErrorName(compressed_size);
    output.write(compression_buffer.data(), compressed_size);
    compressed_sizes_.push_back(compressed_size);
    compressed_bytes_ += compressed_size;

    AdvanceProgress(count);
  }

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