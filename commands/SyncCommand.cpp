#include "SyncCommand.h"

#include <fstream>
#include <future>
#include <map>
#include <utility>

#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

constexpr auto VERIFY = false;

SyncCommand::Impl::Impl(
    const std::string &data_uri,
    const std::string &metadata_uri,
    std::string seed_uri,
    std::ostream &_output)
    : dataReader(Reader::create(data_uri)),
      metadataReader(Reader::create(metadata_uri)),
      seedUri(std::move(seed_uri)),
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
    } else if (key == "hash:") {
      hash = value;
    } else if (key == "eof:") {
      CHECK(value == "1") << "bad eof marker (1)";
      break;
    } else {
      CHECK(false) << "invalid header key `" << key << "`";
    }
  }

  header.read(buffer, 1);
  CHECK(*buffer == '\n') << "bad eof marker (\\n)";

  headerSize = header.tellg();
}

void SyncCommand::Impl::readMetadata()
{
  progressPhase++;
  progressTotalBytes = metadataReader->size();
  progressCurrentBytes = 0;

  auto &offset = progressCurrentBytes;

  parseHeader();
  offset = headerSize;

  size_t count;
  size_t countRead;

  count = blockCount * sizeof(uint32_t);
  weakChecksums.resize(blockCount);
  countRead = metadataReader->read(weakChecksums.data(), offset, count);
  CHECK_EQ(count, countRead) << "cannot read all weak checksums";
  offset += count;

  count = blockCount * sizeof(StrongChecksum);
  strongChecksums.resize(blockCount);
  countRead = metadataReader->read(strongChecksums.data(), offset, count);
  CHECK_EQ(count, countRead) << "cannot read all strong checksums";
  offset += count;

  for (size_t index = 0; index < blockCount; index++) {
    set[weakChecksums[index]] = true;
    analysis[weakChecksums[index]] = {index, -1ull};
  }
}

void SyncCommand::Impl::analyzeSeedCallback(
    const char *buffer,
    size_t offset,
    uint32_t wcs,
    size_t seedOffset,
    const Reader &seedReader)
{
  //  NOTE: (slower) alternative but taking less space:
  //
  //  const auto &found = analysis.find(wcs);
  //  if (found != analysis.end()) {
  //    auto &data = found->second;
  if (set[wcs]) {
    // don't let the matches linger past the end of the seed file
    if (seedOffset + offset + block > progressTotalBytes)
      return;

    auto &data = analysis[wcs];

    weakChecksumMatches++;

    auto sourceDigest = strongChecksums[data.index];
    auto seedDigest = StrongChecksum::compute(buffer + offset, block);

    if (sourceDigest == seedDigest) {
      set[wcs] = false;
      strongChecksumMatches++;
      data.seedOffset = seedOffset + offset;

      if (VERIFY) {
        auto b = std::make_unique<char[]>(block);
        seedReader.read(b.get(), data.seedOffset, block);
        LOG_ASSERT(wcs == weakChecksum(b.get(), block));
        LOG_ASSERT(seedDigest == StrongChecksum::compute(b.get(), block));
      }
    } else {
      weakChecksumFalsePositive++;
    }
  }
}

void SyncCommand::Impl::analyzeSeedChunk(size_t startOffset, size_t endOffset)
{
  auto smartBuffer = std::make_unique<char[]>(2 * block);
  auto buffer = smartBuffer.get() + block;
  memset(buffer, 0, block);

  auto seedReader = Reader::create(seedUri);

  uint32_t wcs = 0;

  bool warmup = true;

  for (size_t seedOffset = startOffset;  //
       seedOffset < endOffset;
       seedOffset += block)
  {
    memcpy(buffer - block, buffer, block);
    auto count = seedReader->read(buffer, seedOffset, block);
    memset(buffer + count, 0, block - count);

    wcs = weakChecksum(
        (const void *)buffer,
        block,
        wcs,
        // FIXME: is there a way to avoid this lambda???
        [this, &buffer, &seedOffset, &seedReader](auto offset, auto wcs) {
          analyzeSeedCallback(buffer, offset, wcs, seedOffset, *seedReader);
        },
        warmup);

    warmup = false;
    progressCurrentBytes += block;
  }
}

void SyncCommand::Impl::analyzeSeed()
{
  auto seedReader = Reader::create(seedUri);

  auto seedDataSize = seedReader->size();
  progressPhase++;
  progressTotalBytes = seedDataSize;
  progressCurrentBytes = 0;

  if (seedDataSize < 128 * block) {
    LOG(INFO) << "using single-threaded version";
    analyzeSeedChunk(0, seedDataSize);
  } else {
    auto threads = 24;
    auto blocks = (seedDataSize + block - 1) / block;
    auto chunk = (blocks + threads - 1) / threads;

    std::vector<std::future<void>> f;

    for (int i = 0; i < threads; i++) {
      auto beg = i * chunk * block;
      auto end = (i + 1) * chunk * block + block;
      LOG(INFO) << "[" << beg << ", " << end << ")";

      f.push_back(std::async(
          [this](auto beg, auto end) { analyzeSeedChunk(beg, end); },
          beg,
          end));
    }

    std::chrono::milliseconds chill(100);
    auto done = false;
    while (!done) {
      done = true;
      for (auto &ff : f) {
        if (ff.wait_for(chill) != std::future_status::ready) {
          done = false;
        }
      }
    }
  }
}

void SyncCommand::Impl::reconstructSource()
{
  progressPhase++;
  progressTotalBytes = dataReader->size();
  progressCurrentBytes = 0;

  StrongChecksumBuilder outputHash;

  auto smartBuffer = std::make_unique<char[]>(block);
  auto buffer = smartBuffer.get();

  auto seedReader = Reader::create(seedUri);

  for (size_t i = 0; i < blockCount; i++) {
    auto wcs = weakChecksums[i];
    auto scs = strongChecksums[i];
    auto data = analysis[wcs];

    size_t count;

    if (data.seedOffset != -1ull && scs == strongChecksums[data.index]) {
      count = seedReader->read(buffer, data.seedOffset, block);
      reusedBytes += count;
    } else {
      count = dataReader->read(buffer, i * block, block);
    }

    outputHash.update(buffer, count);
    output.write(buffer, count);

    if (VERIFY) {
      LOG_IF(ERROR, wcs != weakChecksums[data.index]);
      if (data.seedOffset != -1ull && scs == strongChecksums[data.index]) {
      }
      LOG_IF(ERROR, weakChecksums[i] != weakChecksum(buffer, count));
      LOG_IF(
          ERROR,
          strongChecksums[i] != StrongChecksum::compute(buffer, count));
    }

    if (i < blockCount - 1 || size % block == 0) {
      CHECK_EQ(count, block);
    } else {
      CHECK_EQ(count, size % block);
    }

    progressCurrentBytes += count;
  }

  LOG(INFO) << hash;
  LOG(INFO) << outputHash.digest().toString();
}

int SyncCommand::Impl::run()
{
  progressPhase++;
  readMetadata();
  analyzeSeed();
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
    std::string seed_uri,
    std::ostream &output)
    : pImpl(new Impl(data_uri, metadata_uri, std::move(seed_uri), output))
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
