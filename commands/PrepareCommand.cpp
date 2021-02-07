#include "PrepareCommand.h"

#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

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

  return 0;
}

void PrepareCommand::Impl::accept(MetricVisitor &visitor, PrepareCommand &host)
{
  VISIT(visitor, reader);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
}

PrepareCommand::Impl::Impl(Reader &_reader, size_t _block)
    : reader(_reader), block(_block)
{
}

PrepareCommand::PrepareCommand(Reader &reader, size_t block)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : pImpl(new Impl(reader, block))
{
}

PrepareCommand::~PrepareCommand() = default;

int PrepareCommand::run()
{
  return pImpl->run();
}

void PrepareCommand::accept(MetricVisitor& visitor) {
  return pImpl->accept(visitor, *this);
}
