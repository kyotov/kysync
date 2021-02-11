#include "SyncCommand.h"

#include <fstream>
#include <map>

#include "../checksums/wcs.h"
#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

SyncCommand::Impl::Impl(
    const std::string &data_uri,
    const std::string &metadata_uri,
    std::istream &_input,
    std::ostream &_output)
    : dataReader(Reader::create(data_uri)),
      metadataReader(Reader::create(metadata_uri)),
      input(_input),
      output(_output)
{
}

void SyncCommand::Impl::parseHeader()
{
  constexpr size_t MAX_HEADER_SIZE = 1024;
  char buffer[MAX_HEADER_SIZE];

  metadataReader->read(buffer, 0, MAX_HEADER_SIZE);

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
  countRead = metadataReader->read(weakChecksums.data(), offset, count);
  CHECK_EQ(count, countRead) << "cannot read all weak checksums";

  offset += count;
  count = blockCount * sizeof(StrongChecksum);
  strongChecksums.resize(blockCount);
  countRead = metadataReader->read(strongChecksums.data(), offset, count);
  CHECK_EQ(count, countRead) << "cannot read all strong checksums";

  for (size_t index = 0; index < blockCount; index++) {
    set[weakChecksums[index]] = true;
    analysis[weakChecksums[index]] = {index, -1ull};
  }
}

void SyncCommand::Impl::analyzeSeedCallback(
    const char *buffer,
    size_t offset,
    uint32_t wcs)
{
  //  NOTE: (slower) alternative but taking less space:
  //
  //  const auto &found = analysis.find(wcs);
  //  if (found != analysis.end()) {
  //    auto &data = found->second;
  if (set[wcs]) {
    auto &data = analysis[wcs];

    weakChecksumMatches++;

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

  input.seekg(0, std::ios_base::end);
  progressTotalBytes = input.tellg();

  uint32_t wcs = 0;

  bool warmup = true;

  input.seekg(0);
  while (input) {
    memcpy(buffer - block, buffer, block);
    auto count = input.read(buffer, block).gcount();
    memset(buffer + count, 0, block - count);

    wcs = weakChecksum(
        (const void *)buffer,
        block,
        wcs,
        // FIXME: is there a way to avoid this lambda???
        [this, &buffer](auto offset, auto wcs) {
          analyzeSeedCallback(buffer, offset, wcs);
        },
        warmup);

    warmup = false;
    progressCurrentBytes += block;
  }
}

void SyncCommand::Impl::reconstructSource()
{
  auto smartBuffer = std::make_unique<char[]>(block);
  auto buffer = smartBuffer.get();

  progressTotalBytes = size;
  progressCurrentBytes = 0;

  for (size_t i = 0; i < blockCount; i++) {
    auto wcs = weakChecksums[i];
    auto scs = strongChecksums[i];
    auto data = analysis[wcs];

    size_t count;

    if (data.seedOffset != -1ull && scs == strongChecksums[data.index]) {
      input.clear();
      input.seekg(data.seedOffset);
      count = input.read(buffer, block).gcount();
      reusedBytes += count;
    } else {
      count = dataReader->read(buffer, i * block, block);
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
  progressPhase++;
  readMetadata();
  progressPhase++;
  analyzeSeed();
  progressPhase++;
  reconstructSource();
  progressPhase++;

  return 0;
}

void SyncCommand::Impl::accept(MetricVisitor &visitor, const SyncCommand &host)
{
  VISIT(visitor, *metadataReader);
  VISIT(visitor, *dataReader);
  VISIT(visitor, progressPhase);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
  VISIT(visitor, weakChecksumMatches);
  VISIT(visitor, weakChecksumFalsePositive);
  VISIT(visitor, strongChecksumMatches);
}

SyncCommand::SyncCommand(
    const std::string &data_uri,
    const std::string &metadata_uri,
    std::istream &input,
    std::ostream &output)
    : pImpl(new Impl(data_uri, metadata_uri, input, output))
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
