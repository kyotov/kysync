#include "PrepareCommand.h"

#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

PrepareCommand::Impl::Impl(
    const Reader &_reader,
    size_t _block,
    std::ostream &_output)
    : reader(_reader),
      block(_block),
      output(_output)
{
}

int PrepareCommand::Impl::run()
{
  auto size = reader.size();
  auto blockCount = (size + block - 1) / block;

  auto unique_buffer = std::make_unique<uint8_t[]>(block);
  auto buffer = unique_buffer.get();

  progressTotalBytes = size;
  progressCurrentBytes = 0;

  for (size_t i = 0; i < blockCount; i++) {
    auto count = reader.read(buffer, i * block, block);
    memset(buffer + count, 0, block - count);

    weakChecksums.push_back(weakChecksum(buffer, block));
    strongChecksums.push_back(StrongChecksum::compute(buffer, block));

    progressCurrentBytes += block;
  }

  // produce the ksync metadata output

  char header[1024];
  sprintf(
      header,
      "version: 1\n"
      "size: %llu\n"
      "block: %llu\n"
      "eof: 1\n",
      size,
      block);

  output.write(header, strlen(header));

  output.write(
      reinterpret_cast<const char *>(weakChecksums.data()),
      weakChecksums.size() * sizeof(uint32_t));

  output.write(
      reinterpret_cast<const char *>(strongChecksums.data()),
      strongChecksums.size() * sizeof(StrongChecksum));

  return 0;
}

void PrepareCommand::Impl::accept(
    MetricVisitor &visitor,
    const PrepareCommand &host)
{
  VISIT(visitor, reader);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
}

PrepareCommand::PrepareCommand(
    const Reader &reader,
    size_t block,
    std::ostream &output)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : pImpl(new Impl(reader, block, output))
{
}

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::run()
{
  return pImpl->run();
}

void PrepareCommand::accept(MetricVisitor &visitor) const
{
  return pImpl->accept(visitor, *this);
}
