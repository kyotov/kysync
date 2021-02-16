#include "PrepareCommand.h"

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

PrepareCommand::Impl::Impl(
    std::istream &_input,
    std::ostream &_output,
    size_t _block)
    : input(_input),
      output(_output),
      block(_block)
{
}

int PrepareCommand::Impl::run()
{
  progressPhase++;

  auto unique_buffer = std::make_unique<char[]>(block);
  auto buffer = unique_buffer.get();

  input.seekg(0, std::ios_base::end);
  progressTotalBytes = input.tellg();

  StrongChecksumBuilder hash;

  progressCurrentBytes = 0;

  input.seekg(0);
  while (auto count = input.read(buffer, block).gcount()) {
    memset(buffer + count, 0, block - count);

    hash.update(buffer, count);

    weakChecksums.push_back(weakChecksum(buffer, block));
    strongChecksums.push_back(StrongChecksum::compute(buffer, block));

    progressCurrentBytes += count;
  }

  // produce the ksync metadata output
  progressPhase++;

  char header[1024];
  sprintf(
      header,
      "version: 1\n"
      "size: %llu\n"
      "block: %llu\n"
      "hash: %s\n"
      "eof: 1\n",
      progressCurrentBytes.load(),
      block,
      hash.digest().toString().c_str());

  output.write(header, strlen(header));

  output.write(
      reinterpret_cast<const char *>(weakChecksums.data()),
      weakChecksums.size() * sizeof(uint32_t));

  output.write(
      reinterpret_cast<const char *>(strongChecksums.data()),
      strongChecksums.size() * sizeof(StrongChecksum));

  progressPhase++;
  return 0;
}

void PrepareCommand::Impl::accept(
    MetricVisitor &visitor,
    const PrepareCommand &host)
{
  VISIT(visitor, progressPhase);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
}

PrepareCommand::PrepareCommand(
    std::istream &input,
    std::ostream &output,
    size_t block)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : pImpl(new Impl(input, output, block))
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
