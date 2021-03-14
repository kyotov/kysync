#include "PrepareCommand.h"

#include <cstring>

#include <glog/logging.h>
#include <zstd.h>
#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

PrepareCommand::Impl::Impl(
    std::istream &_input,
    std::ostream &_output_ksync,
    std::ostream &_output_compressed,
    size_t _block,
    Command::Impl &_baseImpl)
    : input(_input),
      output_ksync_(_output_ksync),
      output_compressed_(_output_compressed),
      block(_block),
      baseImpl(_baseImpl)
{  
  compression_buffer_size_ = ZSTD_compressBound(block);
  compression_buffer_ = new char[compression_buffer_size_];
}

int PrepareCommand::Impl::run()
{
  baseImpl.progressPhase++;

  auto unique_buffer = std::make_unique<char[]>(block);
  auto buffer = unique_buffer.get();

  input.seekg(0, std::ios_base::end);
  baseImpl.progressTotalBytes = input.tellg();

  StrongChecksumBuilder hash;

  baseImpl.progressCurrentBytes = 0;

  input.seekg(0);
  while (auto count = input.read(buffer, block).gcount()) {
    memset(buffer + count, 0, block - count);

    hash.update(buffer, count);

    // TODO: Ensure that it's a conscious perf tradeoff to have
    // 3 vectors instead of a single vector of (weakchecksum, strongchecksum, compressoffset) tuples
    weakChecksums.push_back(weakChecksum(buffer, block));
    strongChecksums.push_back(StrongChecksum::compute(buffer, block));

    size_t compressed_size = ZSTD_compress(
      compression_buffer_, 
      compression_buffer_size_, 
      buffer, 
      count, 
      compression_level_);
    if (ZSTD_isError(compressed_size)) {
      LOG(ERROR) << "Error when performing zstd compression: " << ZSTD_getErrorName(compressed_size);
    }
    CHECK(!ZSTD_isError(compressed_size));
    LOG_ASSERT(compressed_size <= compression_buffer_size_);
    output_compressed_.write(compression_buffer_, compressed_size);
    compression_sizes_.push_back(compressed_size);

    baseImpl.progressCurrentBytes += count;
  }

  // produce the ksync metadata output
  baseImpl.progressPhase++;

  char header[1024];
  sprintf(
      header,
      "version: 1\n"
      "size: %lu\n" // "size: %llu\n"
      "block: %lu\n" // "block: %llu\n"
      "hash: %s\n"
      "eof: 1\n",
      baseImpl.progressCurrentBytes.load(),
      block,
      hash.digest().toString().c_str());

  output_ksync_.write(header, strlen(header));

  output_ksync_.write(
      reinterpret_cast<const char *>(weakChecksums.data()),
      weakChecksums.size() * sizeof(uint32_t));

  output_ksync_.write(
      reinterpret_cast<const char *>(strongChecksums.data()),
      strongChecksums.size() * sizeof(StrongChecksum));

  // TODO: This will be enabled after the read logic has been updated
  /*
  output_ksync_.write(
      reinterpret_cast<const char *>(compression_sizes_.data()),
      compression_sizes_.size() * sizeof(compression_sizes_));
  */

  baseImpl.progressPhase++;
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

PrepareCommand::~PrepareCommand()
{
  // TODO: Confirm whether this should be a part of Impl destructor
  delete[] pImpl->compression_buffer_;
}

int PrepareCommand::run()
{
  return pImpl->run();
}

void PrepareCommand::accept(MetricVisitor &visitor) const
{
  Command::accept(visitor);
  pImpl->accept(visitor, *this);
}
