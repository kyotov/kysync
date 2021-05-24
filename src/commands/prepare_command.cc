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
    fs::path input_filename,
    fs::path output_ksync_filename,
    fs::path output_compressed_filename,
    size_t block_size)
    : kInputFilename(std::move(input_filename)),
      kOutputKsyncFilename(
          !output_ksync_filename.empty() ? std::move(output_ksync_filename)
                                         : input_filename.string() + ".kysync"),
      kOutputCompressedFilename(
          !output_compressed_filename.empty()
              ? std::move(output_compressed_filename)
              : input_filename.string() + ".pzst"),
      kBlockSize(block_size) {}

int PrepareCommand::Run() {
  auto unique_buffer = std::make_unique<char[]>(kBlockSize);
  auto *buffer = unique_buffer.get();

  auto compression_buffer_size = ZSTD_compressBound(kBlockSize);
  auto unique_compression_buffer =
      std::make_unique<char[]>(compression_buffer_size);
  auto *compression_buffer = unique_compression_buffer.get();

  auto input = std::ifstream(kInputFilename, std::ios::binary);
  CHECK(input) << "error reading from " << kInputFilename;

  auto output = std::ofstream(kOutputCompressedFilename, std::ios::binary);
  CHECK(output) << "unable to write to " << kOutputCompressedFilename;

  const size_t kDataSize = fs::file_size(kInputFilename);
  StartNextPhase(kDataSize);

  StrongChecksumBuilder hash;

  while (auto count = input.read(buffer, kBlockSize).gcount()) {
    memset(buffer + count, 0, kBlockSize - count);

    hash.Update(buffer, count);

    weak_checksums_.push_back(WeakChecksum(buffer, kBlockSize));
    strong_checksums_.push_back(StrongChecksum::Compute(buffer, kBlockSize));

    size_t compressed_size = ZSTD_compress(
        compression_buffer,
        compression_buffer_size,
        buffer,
        count,
        kCompressionLevel);
    CHECK(!ZSTD_isError(compressed_size)) << ZSTD_getErrorName(compressed_size);
    output.write(compression_buffer, compressed_size);
    compressed_sizes_.push_back(compressed_size);
    compressed_bytes_ += compressed_size;

    AdvanceProgress(count);
  }

  // produce the ksync metadata output

  auto output_ksync = std::ofstream(kOutputKsyncFilename, std::ios::binary);
  CHECK(output_ksync) << "unable to write to " << kOutputKsyncFilename;

  StartNextPhase(1);

  auto header = Header();
  header.set_version(2);
  header.set_size(kDataSize);
  header.set_block_size(kBlockSize);
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