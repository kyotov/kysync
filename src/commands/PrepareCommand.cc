#include "PrepareCommand.h"

#include <glog/logging.h>
#include <zstd.h>

#include <cinttypes>

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

namespace kysync {

PrepareCommand::Impl::Impl(
    const PrepareCommand &parent,
    std::istream &input,
    std::ostream &output_ksync,
    std::ostream &output_compressed,
    size_t block_size)
    : kParent(parent),
      input_(input),
      output_ksync_(output_ksync),
      output_compressed_(output_compressed),
      block_size_(block_size) {}

template <typename T>
void PrepareCommand::Impl::WriteToMetadataStream(
    const std::vector<T> &container) {
  auto size = container.size() * sizeof(typename std::vector<T>::value_type);
  output_ksync_.write(reinterpret_cast<const char *>(container.data()), size);
  kParent.AdvanceProgress(size);
}

int PrepareCommand::Impl::Run() {
  auto unique_buffer = std::make_unique<char[]>(block_size_);
  auto buffer = unique_buffer.get();

  auto compression_buffer_size = ZSTD_compressBound(block_size_);
  auto unique_compression_buffer =
      std::make_unique<char[]>(compression_buffer_size);
  auto compression_buffer = unique_compression_buffer.get();

  input_.seekg(0, std::ios_base::end);
  const size_t kDataSize = input_.tellg();
  kParent.StartNextPhase(kDataSize);

  StrongChecksumBuilder hash;

  input_.seekg(0);
  while (auto count = input_.read(buffer, block_size_).gcount()) {
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
    output_compressed_.write(compression_buffer, compressed_size);
    compressed_sizes_.push_back(compressed_size);
    compressed_bytes_ += compressed_size;

    kParent.AdvanceProgress(count);
  }

  // produce the ksync metadata output

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
      block_size_,
      hash.Digest().ToString().c_str());

  output_ksync_.write(header, strlen(header));
  kParent.AdvanceProgress(strlen(header));

  WriteToMetadataStream(weak_checksums_);
  WriteToMetadataStream(strong_checksums_);
  WriteToMetadataStream(compressed_sizes_);

  kParent.StartNextPhase(0);
  return 0;
}

PrepareCommand::PrepareCommand(
    std::istream &input,
    std::ostream &output_ksync,
    std::ostream &output_compressed,
    size_t block)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : impl_(new Impl(*this, input, output_ksync, output_compressed, block)) {}

PrepareCommand::PrepareCommand(PrepareCommand &&) noexcept = default;

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::Run() { return impl_->Run(); }

void PrepareCommand::Accept(MetricVisitor &visitor) {
  Command::Accept(visitor);
  VISIT_METRICS(impl_->compressed_bytes_);
}

}  // namespace kysync