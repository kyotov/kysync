#include "PrepareCommand.h"

#include <glog/logging.h>
#include <zstd.h>

#include <cinttypes>
#include <fstream>
#include <utility>

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "../utilities/streams.h"
#include "impl/PrepareCommandImpl.h"

namespace kysync {

PrepareCommand::Impl::Impl(
    const PrepareCommand &parent,
    fs::path input_filename,
    fs::path output_ksync_filename,
    fs::path output_compressed_filename,
    size_t block_size)
    : kParent(parent),
      input_filename_(std::move(input_filename)),
      output_ksync_filename_(std::move(output_ksync_filename)),
      output_compressed_filename_(std::move(output_compressed_filename)),
      kBlockSize(block_size) {}
//  CHECK(input_) << "error reading from " << input_filename;
//
//  auto output_ksync =
//      std::ofstream(output_ksync_, std::ios::binary);
//  CHECK(output_ksync) << "unable to write to " << output_ksync_filename;
//
//  auto output_compressed =
//      std::ofstream(output_compressed_, std::ios::binary);
//  CHECK(output_compressed) << "unable to write to "
//                           << output_compressed_filename;

int PrepareCommand::Impl::Run() {
  auto unique_buffer = std::make_unique<char[]>(kBlockSize);
  auto buffer = unique_buffer.get();

  auto compression_buffer_size = ZSTD_compressBound(kBlockSize);
  auto unique_compression_buffer =
      std::make_unique<char[]>(compression_buffer_size);
  auto compression_buffer = unique_compression_buffer.get();

  auto input = std::ifstream(input_filename_, std::ios::binary);
  CHECK(input) << "error reading from " << input_filename_;

  auto output = std::ofstream(output_compressed_filename_, std::ios::binary);
  CHECK(output) << "unable to write to " << output_compressed_filename_;

  const size_t kDataSize = fs::file_size(input_filename_);
  kParent.StartNextPhase(kDataSize);

  StrongChecksumBuilder hash;

  input.seekg(0);
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

  auto output_ksync = std::ofstream(output_ksync_filename_, std::ios::binary);
  CHECK(output_ksync) << "unable to write to " << output_ksync_filename_;

  kParent.StartNextPhase(1);

  char header[1024];

  sprintf(
      header,
      "version: 1\n"
      "size: %" PRIu64
      "\n"
      "block: %" PRIu64
      "\n"
      "hash: %s\n"
      "eof: 1\n",
      kDataSize,
      kBlockSize,
      hash.Digest().ToString().c_str());

  output_ksync.write(header, strlen(header));
  kParent.AdvanceProgress(strlen(header));

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
          !output_ksync_filename.empty() ? output_ksync_filename
                                         : input_filename.string() + ".kysync",
          !output_compressed_filename.empty()
              ? output_compressed_filename
              : input_filename.string() + ".pzst",
          block_size)){};

PrepareCommand::PrepareCommand(PrepareCommand &&) noexcept = default;

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::Run() { return impl_->Run(); }

void PrepareCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  VISIT_METRICS(impl_->compressed_bytes_);
}

}  // namespace kysync