#include "PrepareCommand.h"

#include <cstring>
#include <cinttypes>
#include <glog/logging.h>
#include <zstd.h>

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

PrepareCommand::Impl::Impl(
    std::istream &input,
    std::ostream &output_ksync,
    std::ostream &output_compressed,
    size_t block_size,
    Command::Impl &baseImpl)
    : input_(input),
      output_ksync_(output_ksync),
      output_compressed_(output_compressed),
      block_size_(block_size),
      base_impl_(baseImpl)
{  
}

int PrepareCommand::Impl::run()
{
  base_impl_.progressPhase++;

  auto unique_buffer = std::make_unique<char[]>(block_size_);
  auto buffer = unique_buffer.get();

  auto compression_buffer_size = ZSTD_compressBound(block_size_);
  auto unique_compression_buffer = std::make_unique<char[]>(compression_buffer_size);
  auto compression_buffer = unique_compression_buffer.get();

  input_.seekg(0, std::ios_base::end);
  base_impl_.progressTotalBytes = input_.tellg();

  StrongChecksumBuilder hash;

  base_impl_.progressCurrentBytes = 0;

  input_.seekg(0);
  while (auto count = input_.read(buffer, block_size_).gcount()) {
    memset(buffer + count, 0, block_size_ - count);

    hash.update(buffer, count);

    weakChecksums.push_back(weakChecksum(buffer, block_size_));
    strongChecksums.push_back(StrongChecksum::compute(buffer, block_size_));

    size_t compressed_size = ZSTD_compress(
      compression_buffer, 
      compression_buffer_size,
      buffer, 
      count, 
      compression_level_);
    CHECK(!ZSTD_isError(compressed_size)) << "Error when performing zstd compression: " << ZSTD_getErrorName(compressed_size);
    LOG_ASSERT(compressed_size <= compression_buffer_size);
    output_compressed_.write(compression_buffer, compressed_size);
    compressed_sizes_.push_back(compressed_size);

    base_impl_.progressCurrentBytes += count;
  }

  // produce the ksync metadata output
  base_impl_.progressPhase++;

  char header[1024];

  sprintf(
      header,
      "version: 1\n"
      "size: %" PRIu64 "\n"
      "block: %" PRIu64 "\n"
      "hash: %s\n"
      "eof: 1\n",
      base_impl_.progressCurrentBytes.load(),
      block_size_,
      hash.digest().toString().c_str());

  output_ksync_.write(header, strlen(header));

  output_ksync_.write(
      reinterpret_cast<const char *>(weakChecksums.data()),
      weakChecksums.size() * sizeof(uint32_t));

  output_ksync_.write(
      reinterpret_cast<const char *>(strongChecksums.data()),
      strongChecksums.size() * sizeof(StrongChecksum));

  output_ksync_.write(
      reinterpret_cast<const char *>(compressed_sizes_.data()),
      compressed_sizes_.size() * sizeof(uint64_t));

  base_impl_.progressPhase++;
  return 0;
}

void PrepareCommand::Impl::accept(
    MetricVisitor &visitor,
    const PrepareCommand &host)
{
}

PrepareCommand::PrepareCommand(
    std::istream &input,
    std::ostream &output_ksync,
    std::ostream &output_compressed,
    size_t block)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : pImpl(new Impl(input, output_ksync, output_compressed, block, *Command::pImpl))
{
}

PrepareCommand::PrepareCommand(PrepareCommand &&) noexcept = default;

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::run()
{
  return pImpl->run();
}

void PrepareCommand::accept(MetricVisitor &visitor) const
{
  Command::accept(visitor);
  pImpl->accept(visitor, *this);
}
