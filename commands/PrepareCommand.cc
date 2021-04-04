#include "PrepareCommand.h"

#include <glog/logging.h>
#include <zstd.h>

#include <cinttypes>

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

PrepareCommand::Impl::Impl(
    std::istream &input,
    std::ostream &output_ksync,
    std::ostream &output_compressed,
    size_t block_size,
    Command::Impl &base_impl)
    : input_(input),
      output_ksync_(output_ksync),
      output_compressed_(output_compressed),
      block_size_(block_size),
      base_impl_(base_impl) {}

template <typename T>
void PrepareCommand::Impl::WriteToMetadataStream(
    const std::vector<T> &container) {
  output_ksync_.write(
      reinterpret_cast<const char *>(container.data()),
      container.size() * sizeof(typename std::vector<T>::value_type));
}

int PrepareCommand::Impl::Run() {
  base_impl_.progress_phase_++;

  auto unique_buffer = std::make_unique<char[]>(block_size_);
  auto buffer = unique_buffer.get();

  auto compression_buffer_size = ZSTD_compressBound(block_size_);
  auto unique_compression_buffer =
      std::make_unique<char[]>(compression_buffer_size);
  auto compression_buffer = unique_compression_buffer.get();

  input_.seekg(0, std::ios_base::end);
  base_impl_.progress_total_bytes_ = input_.tellg();

  StrongChecksumBuilder hash;

  base_impl_.progress_current_bytes_ = 0;
  base_impl_.progress_compressed_bytes_ = 0;

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
    base_impl_.progress_compressed_bytes_ += compressed_size;

    base_impl_.progress_current_bytes_ += count;
  }

  // produce the ksync metadata output
  base_impl_.progress_phase_++;

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
      base_impl_.progress_current_bytes_.load(),
      block_size_,
      hash.Digest().ToString().c_str());

  output_ksync_.write(header, strlen(header));

  WriteToMetadataStream(weak_checksums_);
  WriteToMetadataStream(strong_checksums_);
  WriteToMetadataStream(compressed_sizes_);

  base_impl_.progress_phase_++;
  return 0;
}

void PrepareCommand::Impl::Accept(
    MetricVisitor &visitor,
    const PrepareCommand &host) {}

PrepareCommand::PrepareCommand(
    std::istream &input,
    std::ostream &output_ksync,
    std::ostream &output_compressed,
    size_t block)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : impl_(new Impl(
          input,
          output_ksync,
          output_compressed,
          block,
          *Command::impl_)) {}

PrepareCommand::PrepareCommand(PrepareCommand &&) noexcept = default;

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::Run() { return impl_->Run(); }

void PrepareCommand::Accept(MetricVisitor &visitor) const {
  Command::Accept(visitor);
  impl_->Accept(visitor, *this);
}
