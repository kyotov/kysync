#include "SyncCommand.h"

#include <fstream>
#include <future>
#include <map>
#include <utility>

#include "../Config.h"
#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

constexpr auto VERIFY = false;
constexpr auto CONCISE_AND_SLOW = false;

SyncCommand::Impl::Impl(
    std::string data_uri,
    const std::string &metadata_uri,
    std::string seed_uri,
    std::filesystem::path output_path,
    int _threads)
    : dataUri(std::move(data_uri)),
      metadataReader(Reader::create(metadata_uri)),
      seedUri(std::move(seed_uri)),
      outputPath(std::move(output_path)),
      threads(_threads)
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
    const uint8_t *buffer,
    size_t offset,
    uint32_t wcs,
    size_t seedOffset,
    const Reader &seedReader,
    int64_t &warmup)
{
  //  NOTE: (slower) alternative but taking less space:
  //
  //  const auto &found = analysis.find(wcs);
  //  if (found != analysis.end()) {
  //    auto &data = found->second;
  if (set[wcs]) {
    // don't let the matches linger past the end of the seed file
    if (seedOffset + offset + block > progressTotalBytes) {
      return;
    }

    auto &data = analysis[wcs];

    weakChecksumMatches++;

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

void SyncCommand::Impl::analyzeSeedChunk(
    int id,
    size_t startOffset,
    size_t endOffset)
{
  auto smartBuffer = std::make_unique<uint8_t[]>(2 * block);
  auto buffer = smartBuffer.get() + block;
  memset(buffer, 0, block);

  auto seedReader = Reader::create(seedUri);
  auto seedSize = seedReader->size();

  uint32_t wcs = 0;

  uint16_t a = 0;
  uint16_t b = 0;

  int64_t warmup = block - 1;

  for (size_t seedOffset = startOffset;  //
       seedOffset < endOffset;
       seedOffset += block)
  {
    memcpy(buffer - block, buffer, block);
    auto count = seedReader->read(buffer, seedOffset, block);
    memset(buffer + count, 0, block - count);

    if (CONCISE_AND_SLOW) {
      wcs = weakChecksum(
          (const void *)buffer,
          block,
          wcs,
          // FIXME: is there a way to avoid this lambda???
          [&](auto offset, auto wcs) {
            analyzeSeedCallback(
                buffer,
                offset,
                wcs,
                seedOffset,
                *seedReader,
                warmup);
          },
          warmup);
    } else {
      auto cb = [&](size_t i) {
        if (--warmup < 0) {
          uint32_t wcs = b << 16 | a;
          if (set[wcs]) {
            auto offset = i + 1 - block;

            auto &data = analysis[wcs];

            weakChecksumMatches++;

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
                LOG_ASSERT(
                    seedDigest == StrongChecksum::compute(t.get(), block));
              }
            } else {
              weakChecksumFalsePositive++;
            }
          }
        }
      };

      auto iteration = [&](size_t i) {
        if (i < block && seedOffset + i < seedSize) {
          a += buffer[i] - buffer[i - block];
          b += a - block * buffer[i - block];
          cb(i);
        }
      };

      for (auto i = 0; i < block; i += 1) {
        iteration(i + 0);
        //        iteration(i + 1);
        //        iteration(i + 2);
        //        iteration(i + 3);
        //        iteration(i + 4);
        //        iteration(i + 5);
        //        iteration(i + 6);
        //        iteration(i + 7);
      }
    }

    progressCurrentBytes += block;
  }
}

int parallelize(
    size_t dataSize,
    size_t blockSize,
    size_t overlapSize,
    int threads,
    std::function<void(int i, size_t startOffset, size_t endOffset)> f)
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

    fs.push_back(std::async(f, id, beg, end));
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
      LOG_IF(ERROR, wcs != weakChecksums[data.index]);
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
  readMetadata();
  analyzeSeed();
  reconstructSource();
  return 0;
}

void SyncCommand::Impl::accept(MetricVisitor &visitor, const SyncCommand &host)
{
  VISIT(visitor, *metadataReader);
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
    const std::string &data_uri,
    const std::string &metadata_uri,
    std::string seed_uri,
    std::filesystem::path output_path,
    int threads)
    : pImpl(new Impl(
          data_uri,
          metadata_uri,
          std::move(seed_uri),
          std::move(output_path),
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
