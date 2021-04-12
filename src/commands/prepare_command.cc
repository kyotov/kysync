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
#include "impl/prepare_command_impl.h"

namespace kysync {

PrepareCommand::Impl::Impl(
    const PrepareCommand &parent,
    fs::path input_filename,
    fs::path output_ksync_filename,
    fs::path output_compressed_filename,
    size_t block_size)
    : kParent(parent),
      kInputFilename(std::move(input_filename)),
      kOutputKsyncFilename(std::move(output_ksync_filename)),
      kOutputCompressedFilename(std::move(output_compressed_filename)),
      kBlockSize(block_size) {}

int PrepareCommand::Impl::Run() {
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
  kParent.StartNextPhase(kDataSize);

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

    kParent.AdvanceProgress(count);
  }

  // produce the ksync metadata output

  auto output_ksync = std::ofstream(kOutputKsyncFilename, std::ios::binary);
  CHECK(output_ksync) << "unable to write to " << kOutputKsyncFilename;

  kParent.StartNextPhase(1);

  auto header = Header();
  header.set_version(2);
  header.set_size(kDataSize);
  header.set_block_size(kBlockSize);
  header.set_hash(hash.Digest().ToString());
  google::protobuf::util::SerializeDelimitedToOstream(header, &output_ksync);
  kParent.AdvanceProgress(header.ByteSizeLong());

  kParent.AdvanceProgress(StreamWrite(output_ksync, weak_checksums_));
  kParent.AdvanceProgress(StreamWrite(output_ksync, strong_checksums_));
  kParent.AdvanceProgress(StreamWrite(output_ksync, compressed_sizes_));

  kParent.StartNextPhase(0);
  return 0;
}

PrepareCommand::PrepareCommand(
    const fs::path &input_filename,
    const fs::path &output_ksync_filename,
    const fs::path &output_compressed_filename,
    size_t block_size)
    : impl_(new Impl(
          *this,
          input_filename,
          !output_ksync_filename.empty() ? output_ksync_filename.string()
                                         : input_filename.string() + ".kysync",
          !output_compressed_filename.empty()
              ? output_compressed_filename.string()
              : input_filename.string() + ".pzst",
          block_size)){};

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::Run() { return impl_->Run(); }

void PrepareCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  VISIT_METRICS(impl_->compressed_bytes_);
}

}  // namespace kysync