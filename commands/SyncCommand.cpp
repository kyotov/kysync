#include "SyncCommand.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "../checksums/wcs.h"
#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

SyncCommand::Impl::Impl(
    const Reader &_metadataReader,
    const Reader &_dataReader,
    const Reader &_seedReader,
    const std::filesystem::path &_outputPath)
    : metadataReader(_metadataReader),
      dataReader(_dataReader),
      seedReader(_seedReader),
      outputPath(_outputPath)
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

  for (size_t index = 0; index < blockCount; index++) {
    analysis[weakChecksums[index]] = {index, -1ull};
  }
}

void SyncCommand::Impl::analyzeSeedCallback(
    const char *buffer,
    size_t offset,
    uint32_t wcs)
{
  const auto &found = analysis.find(wcs);
  if (found != analysis.end()) {
    weakChecksumMatches++;
    auto &data = found->second;

    auto sourceDigest = strongChecksums[data.index];
    auto seedDigest = StrongChecksum::compute(buffer + offset, block);

    if (sourceDigest == seedDigest) {
      strongChecksumMatches++;
      data.seedOffset = progressCurrentBytes + offset;
    } else {
      weakChecksumFalsePositive++;
    }
  }
}

void SyncCommand::Impl::analyzeSeed()
{
  auto smartBuffer = std::make_unique<char[]>(2 * block);
  auto buffer = smartBuffer.get() + block;
  memset(buffer, 0, block);

  auto seedSize = seedReader.size();
  auto seedBlockCount = (seedSize + block - 1) / block;

  uint32_t wcs = 0;

  progressTotalBytes = seedSize;
  for (size_t i = 0; i < seedBlockCount; i++) {
    memcpy(buffer - block, buffer, block);
    memset(buffer, 0, block);
    seedReader.read(buffer, i * block, block);

    wcs = weakChecksum(
        (const void *)buffer,
        block,
        wcs,
        // FIXME: is there a way to avoid this lambda???
        [this, &buffer](auto offset, auto wcs) {
          analyzeSeedCallback(buffer, offset, wcs);
        },
        i == 0);
    progressCurrentBytes += block;
  }
}

void SyncCommand::Impl::reconstructSource()
{
  auto output = std::ofstream(outputPath, std::ios::binary);

  auto smartBuffer = std::make_unique<char[]>(block);
  auto buffer = smartBuffer.get();

  progressTotalBytes = size.load();
  progressCurrentBytes = 0;
  for (size_t i = 0; i < blockCount; i++) {
    auto wcs = weakChecksums[i];
    auto scs = strongChecksums[i];
    auto data = analysis[wcs];

    auto reused =
        data.seedOffset != -1ull && scs == strongChecksums[data.index];

    auto count = reused ? seedReader.read(buffer, data.seedOffset, block)
                        : dataReader.read(buffer, i * block, block);

    if (reused) {
      reusedBytes += count;
    }

    output.write(buffer, count);

    if (i < blockCount - 1 || size % block == 0) {
      CHECK_EQ(count, block);
    } else {
      CHECK_EQ(count, size % block);
    }

    progressCurrentBytes += count;
  }
}

int SyncCommand::Impl::run()
{
  readMetadata();
  analyzeSeed();
  reconstructSource();

  return 0;
}

void SyncCommand::Impl::accept(MetricVisitor &visitor, const SyncCommand &host)
{
  VISIT(visitor, metadataReader);
  VISIT(visitor, dataReader);
  VISIT(visitor, seedReader);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
  VISIT(visitor, weakChecksumMatches);
  VISIT(visitor, weakChecksumFalsePositive);
  VISIT(visitor, strongChecksumMatches);
}

SyncCommand::SyncCommand(
    const Reader &metadataReader,
    const Reader &dataReader,
    const Reader &seedReader,
    const std::filesystem::path &outputPath)
    : pImpl(new Impl(metadataReader, dataReader, seedReader, outputPath))
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
