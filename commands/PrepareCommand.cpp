#include "PrepareCommand.h"

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "impl/PrepareCommandImpl.h"

PrepareCommand::Impl::Impl(
    std::istream &_input,
    std::ostream &_output,
    size_t _block,
    Command::Impl &_baseImpl)
    : input(_input),
      output(_output),
      block(_block),
      baseImpl(_baseImpl)
{
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

    weakChecksums.push_back(weakChecksum(buffer, block));
    strongChecksums.push_back(StrongChecksum::compute(buffer, block));

    baseImpl.progressCurrentBytes += count;
  }

  // produce the ksync metadata output
  baseImpl.progressPhase++;

  char header[1024];
  sprintf(
      header,
      "version: 1\n"
      "size: %llu\n"
      "block: %llu\n"
      "hash: %s\n"
      "eof: 1\n",
      baseImpl.progressCurrentBytes.load(),
      block,
      hash.digest().toString().c_str());

  output.write(header, strlen(header));

  output.write(
      reinterpret_cast<const char *>(weakChecksums.data()),
      weakChecksums.size() * sizeof(uint32_t));

  output.write(
      reinterpret_cast<const char *>(strongChecksums.data()),
      strongChecksums.size() * sizeof(StrongChecksum));

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
    std::ostream &output,
    size_t block)
    // FIXME: due to the privacy declarations, we can't use std::make_unique??
    : pImpl(new Impl(input, output, block, *Command::pImpl))
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
