#include "SyncCommand.h"

#include <map>
#include <sstream>

#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

SyncCommand::Impl::Impl(
    const Reader &_metadataReader,
    const Reader &_dataReader)
    : metadataReader(_metadataReader),
      dataReader(_dataReader)
{
}

void SyncCommand::Impl::parseHeader()
{
  constexpr size_t MAX_HEADER_SIZE = 1024;
  char buffer[MAX_HEADER_SIZE];

  metadataReader.read(buffer, 0, MAX_HEADER_SIZE);

  std::stringstream header;
  header.write(buffer, MAX_HEADER_SIZE);
  header.seekg(0);

  std::map<std::string, std::string> metadata;

  std::string key;
  std::string value;

  while (true) {
    header >> key >> value;
    if (key == "version:") {
      CHECK(value == "1") << "unsupported version " << value;
    } else if (key == "size:") {
      size = strtoull(value.c_str(), nullptr, 10);
    } else if (key == "block:") {
      block = strtoull(value.c_str(), nullptr, 10);
      blockCount = (size + block - 1) / block;
    } else if (key == "eof:") {
      CHECK(value == "1") << "bad eof marker (1)";
      break;
    } else {
      CHECK(false) << "invalid header key" << key;
    }
  }

  header.read(buffer, 1);
  CHECK(*buffer == '\n') << "bad eof marker (\\n)";

  headerSize = header.tellg();
}

void SyncCommand::Impl::readMetadata()
{
  parseHeader();

  size_t offset;
  size_t count;
  size_t countRead;

  offset = headerSize;
  count = blockCount * sizeof(uint32_t);
  weakChecksums.resize(blockCount);
  countRead = metadataReader.read(weakChecksums.data(), offset, count);
  CHECK_EQ(count, countRead) << "cannot read all weak checksums";

  offset += count;
  count = blockCount * sizeof(StrongChecksum);
  strongChecksums.resize(blockCount);
  countRead = metadataReader.read(strongChecksums.data(), offset, count);
  CHECK_EQ(count, countRead) << "cannot read all strong checksums";
}

int SyncCommand::Impl::run()
{
  readMetadata();

  //TODO: finish implementation

  return 0;
}

void SyncCommand::Impl::accept(MetricVisitor &visitor, const SyncCommand &host)
{
  VISIT(visitor, metadataReader);
  VISIT(visitor, dataReader);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
}

SyncCommand::SyncCommand(const Reader &metadataReader, const Reader &dataReader)
    : pImpl(new Impl(metadataReader, dataReader))
{
}

SyncCommand::~SyncCommand() = default;

int SyncCommand::run()
{
  return pImpl->run();
}

void SyncCommand::accept(MetricVisitor &visitor) const
{
  pImpl->accept(visitor, *this);
}
