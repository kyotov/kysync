#include "SyncCommand.h"

#include <fstream>
#include <future>
#include <map>

#include "../Config.h"
#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

SyncCommand::Impl::Impl(
    std::string _dataUri,
    std::string _metadataUri,
    std::string _seedUri,
    std::filesystem::path _outputPath,
    int _threads)
    : dataUri(std::move(_dataUri)),
      metadataUri(std::move(_metadataUri)),
      seedUri(std::move(_seedUri)),
      outputPath(std::move(_outputPath)),
      threads(_threads)
{
}

void SyncCommand::Impl::parseHeader(const Reader &metadataReader)
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
  auto metadataReader = Reader::create(metadataUri);

  progressPhase++;
  progressTotalBytes = metadataReader->size();
  progressCurrentBytes = 0;

  auto &offset = progressCurrentBytes;

  parseHeader(*metadataReader);
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

void SyncCommand::Impl::analyzeSeedChunk(
    int /*id*/,
    size_t startOffset,
    size_t endOffset)
{
  auto smartBuffer = std::make_unique<uint8_t[]>(2 * block);
  auto buffer = smartBuffer.get() + block;
  memset(buffer, 0, block);

  auto seedReader = Reader::create(seedUri);
  auto seedSize = seedReader->size();

  uint32_t _wcs = 0;

  int64_t warmup = block - 1;

  for (size_t seedOffset = startOffset;  //
       seedOffset < endOffset;
       seedOffset += block)
  {
    auto callback = [&](size_t offset, uint32_t wcs) {
      /* The `set` seems to improve performance. Previously the code was:
       * https://github.com/kyotov/ksync/blob/2d98f83cd1516066416e8319fbfa995e3f49f3dd/commands/SyncCommand.cpp#L128-L132
       */
      if (--warmup < 0 && seedOffset + offset + block <= seedSize && set[wcs]) {
        weakChecksumMatches++;
        auto &data = analysis[wcs];

        auto sourceDigest = strongChecksums[data.index];
        auto seedDigest = StrongChecksum::compute(buffer + offset, block);

        if (sourceDigest == seedDigest) {
          set[wcs] = false;
          if (WARMUP_AFTER_MATCH) {
            warmup = block - 1;
          }
          strongChecksumMatches++;
          data.seedOffset = seedOffset + offset;

          if (VERIFY) {
            auto t = std::make_unique<char[]>(block);
            seedReader->read(t.get(), data.seedOffset, block);
            LOG_ASSERT(wcs == weakChecksum(t.get(), block));
            LOG_ASSERT(seedDigest == StrongChecksum::compute(t.get(), block));
          }
        } else {
          weakChecksumFalsePositive++;
        }
      }
    };

    memcpy(buffer - block, buffer, block);
    auto count = seedReader->read(buffer, seedOffset, block);
    memset(buffer + count, 0, block - count);

    /* I tried to "optimize" the following by manually inlining `weakChecksum`
     * and then unrolling the inner loop. To my surprise this did not help...
     * The MSVC compiler must be already doing it...
     * We should verify that other compilers do reasonably well before we
     * abandon the idea completely.
     * https://github.com/kyotov/ksync/blob/2d98f83cd1516066416e8319fbfa995e3f49f3dd/commands/SyncCommand.cpp#L166-L220
     */
    _wcs = weakChecksum((const void *)buffer, block, _wcs, callback);

    progressCurrentBytes += block;
  }
}

// TODO: make this a member function
int parallelize(
    size_t dataSize,
    size_t blockSize,
    size_t overlapSize,
    int threads,
    std::function<void(int /*id*/, size_t /*beg*/, size_t /*end*/)> f)
{
  auto blocks = (dataSize + blockSize - 1) / blockSize;
  auto chunk = (blocks + threads - 1) / threads;

  if (chunk < 2) {
    LOG(INFO) << "size too small... not using parallelization";
    threads = 1;
    chunk = blocks;
  }

  VLOG(1) << "parallelize size=" << dataSize  //
          << " block=" << blockSize           //
          << " threads=" << threads;

  std::vector<std::future<void>> fs;

  for (int id = 0; id < threads; id++) {
    auto beg = id * chunk * blockSize;
    auto end = (id + 1) * chunk * blockSize + overlapSize;
    VLOG(2) << "thread=" << id << " [" << beg << ", " << end << ")";

    fs.push_back(std::async(f, id, beg, std::min(end, dataSize)));
  }

  std::chrono::milliseconds chill(100);
  auto done = false;
  while (!done) {
    done = true;
    for (auto &ff : fs) {
      if (ff.wait_for(chill) != std::future_status::ready) {
        done = false;
      }
    }
  }

  return threads;
}

void SyncCommand::Impl::analyzeSeed()
{
  auto seedReader = Reader::create(seedUri);

  auto seedDataSize = seedReader->size();
  progressPhase++;
  progressTotalBytes = seedDataSize;
  progressCurrentBytes = 0;

  parallelize(
      seedDataSize,
      block,
      block,
      threads,
      // TODO: fold this function in here so we would not need the lambda
      [this](auto id, auto beg, auto end) { analyzeSeedChunk(id, beg, end); });
}

std::filesystem::path getChunkPath(std::filesystem::path p, int i)
{
  std::stringstream t;
  t << "." << i;
  return p.concat(t.str());
}

void SyncCommand::Impl::reconstructSourceChunk(
    int id,
    size_t begOffset,
    size_t endOffset)
{
  LOG_ASSERT(begOffset % block == 0);

  auto smartBuffer = std::make_unique<char[]>(block);
  auto buffer = smartBuffer.get();

  auto seedReader = Reader::create(seedUri);
  auto dataReader = Reader::create(dataUri);

  std::ofstream output(getChunkPath(outputPath, id), std::ios::binary);

  for (auto offset = begOffset; offset < endOffset; offset += block) {
    auto i = offset / block;
    auto wcs = weakChecksums[i];
    auto scs = strongChecksums[i];
    auto data = analysis[wcs];

    size_t count;

    if (data.seedOffset != -1ull && scs == strongChecksums[data.index]) {
      count = seedReader->read(buffer, data.seedOffset, block);
      reusedBytes += count;
    } else {
      count = dataReader->read(buffer, i * block, block);
      downloadedBytes += count;
    }

    output.write(buffer, count);

    if (VERIFY) {
      LOG_ASSERT(wcs == weakChecksums[data.index]);
      if (count == block) {
        LOG_ASSERT(wcs == weakChecksum(buffer, count));
        LOG_ASSERT(scs == StrongChecksum::compute(buffer, count));
      }
    }

    if (i < blockCount - 1 || size % block == 0) {
      CHECK_EQ(count, block);
    } else {
      CHECK_EQ(count, size % block);
    }

    progressCurrentBytes += count;
  }
}

void SyncCommand::Impl::reconstructSource()
{
  auto dataReader = Reader::create(dataUri);
  auto dataSize = dataReader->size();

  progressPhase++;
  progressTotalBytes = dataSize;
  progressCurrentBytes = 0;

  auto actualThreads = parallelize(
      dataSize,
      block,
      0,
      threads,
      [this](auto id, auto beg, auto end) {
        reconstructSourceChunk(id, beg, end);
      });

  progressPhase++;
  progressTotalBytes = dataSize;
  progressCurrentBytes = 0;

  constexpr auto bufferSize = 1024 * 1024;
  auto smartBuffer = std::make_unique<char[]>(bufferSize);
  auto buffer = smartBuffer.get();

  StrongChecksumBuilder outputHash;

  std::ofstream output(outputPath, std::ios::binary);

  for (int id = 0; id < actualThreads; id++) {
    auto chunkPath = getChunkPath(outputPath, id);
    {
      std::ifstream chunk(chunkPath, std::ios::binary);
      while (chunk) {
        auto count = chunk.read(buffer, bufferSize).gcount();
        output.write(buffer, count);
        outputHash.update(buffer, count);
        progressCurrentBytes += count;
      }
    }
    std::filesystem::remove(chunkPath);
  }

  CHECK_EQ(hash, outputHash.digest().toString())
      << "mismatch in hash of reconstructed data";
}

int SyncCommand::Impl::run()
{
  LOG(INFO) << "reading metadata...";
  readMetadata();
  LOG(INFO) << "analyzing seed data...";
  analyzeSeed();
  LOG(INFO) << "reconstructing target...";
  reconstructSource();
  return 0;
}

void SyncCommand::Impl::accept(MetricVisitor &visitor, const SyncCommand &host)
{
  VISIT(visitor, progressPhase);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
  VISIT(visitor, weakChecksumMatches);
  VISIT(visitor, weakChecksumFalsePositive);
  VISIT(visitor, strongChecksumMatches);
  VISIT(visitor, reusedBytes);
  VISIT(visitor, downloadedBytes);
}

SyncCommand::SyncCommand(
    std::string dataUri,
    std::string metadataUri,
    std::string seedUri,
    std::filesystem::path outputPath,
    int threads)
    : pImpl(new Impl(
          std::move(dataUri),
          std::move(metadataUri),
          std::move(seedUri),
          std::move(outputPath),
          threads))
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
