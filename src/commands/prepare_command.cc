#include "prepare_command.h"

#include <glog/logging.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <src/commands/pb/header.pb.h>
#include <zstd.h>

#include <cinttypes>
#include <fstream>
#include <utility>

#include "../checksums/strong_checksum_builder.h"
#include "../checksums/weak_checksum.h"
#include "../utilities/streams.h"

namespace kysync {

PrepareCommand::PrepareCommand(
    fs::path input_file_path,
    fs::path output_ksync_file_path,
    fs::path output_compressed_file_path,
    size_t block_size)
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
  auto unique_buffer = std::make_unique<char[]>(block_size_);
  auto *buffer = unique_buffer.get();

  auto compression_buffer_size = ZSTD_compressBound(block_size_);
  auto unique_compression_buffer =
      std::make_unique<char[]>(compression_buffer_size);
  auto *compression_buffer = unique_compression_buffer.get();

  auto input = std::ifstream(input_file_path_, std::ios::binary);
  CHECK(input) << "error reading from " << input_file_path_;

  auto output = std::ofstream(output_compressed_file_path_, std::ios::binary);
  CHECK(output) << "unable to write to " << output_compressed_file_path_;

  const size_t data_size = fs::file_size(input_file_path_);
  StartNextPhase(data_size);

  StrongChecksumBuilder hash;

  while (auto count = input.read(buffer, block_size_).gcount()) {
    memset(buffer + count, 0, block_size_ - count);

    hash.Update(buffer, count);

    weak_checksums_.push_back(WeakChecksum(buffer, block_size_));
    strong_checksums_.push_back(StrongChecksum::Compute(buffer, block_size_));

    size_t compressed_size = ZSTD_compress(
        compression_buffer,
        compression_buffer_size,
        buffer,
        count,
        compression_level_);
    CHECK(!ZSTD_isError(compressed_size)) << ZSTD_getErrorName(compressed_size);
    output.write(compression_buffer, compressed_size);
    compressed_sizes_.push_back(compressed_size);
    compressed_bytes_ += compressed_size;

    AdvanceProgress(count);
  }

  // produce the ksync metadata output

  auto output_ksync = std::ofstream(output_ksync_file_path_, std::ios::binary);
  CHECK(output_ksync) << "unable to write to " << output_ksync_file_path_;

  StartNextPhase(1);

  auto header = Header();
  header.set_version(2);
  header.set_size(data_size);
  header.set_block_size(block_size_);
  header.set_hash(hash.Digest().ToString());
  google::protobuf::util::SerializeDelimitedToOstream(header, &output_ksync);
  AdvanceProgress(header.ByteSizeLong());

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