#include "SyncCommand.h"

#include <fstream>
#include <future>
#include <map>
#include <zstd.h>

#include "../Config.h"
#include "../checksums/StrongChecksumBuilder.h"
#include "../checksums/wcs.h"
#include "glog/logging.h"
#include "impl/SyncCommandImpl.h"

SyncCommand::Impl::Impl(
    std::string data_uri,
    bool compression_disabled,
    std::string metadata_uri,
    std::string seed_uri,
    std::filesystem::path output_path,
    int threads,
    Command::Impl &base_impl)
    : dataUri(std::move(data_uri)),
      compression_diabled_(compression_disabled),
      metadataUri(std::move(metadata_uri)),
      seedUri(std::move(seed_uri)),
      outputPath(std::move(output_path)),
      threads_(threads),
      baseImpl(base_impl)
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

template<typename T>
size_t SyncCommand::Impl::ReadMetadataIntoContainer(const Reader& metadata_reader, size_t offset, std::vector<T>& container)
{
  size_t size_to_read = blockCount * sizeof(typename std::vector<T>::value_type);
  container.resize(blockCount);
  size_t size_read = metadata_reader.read(container.data(), offset, size_to_read);
  CHECK_EQ(size_to_read, size_read) << "cannot read metadata";
  return size_read;
}

void SyncCommand::Impl::UpdateCompressedOffsetsAndMaxSize() 
{
  compressed_file_offsets_.push_back(0);
  if (compressed_sizes_.size() > 0) {
    max_compressed_size_ = compressed_sizes_[0];
  }
  for (int i = 1; i < blockCount; i++) {
    compressed_file_offsets_.push_back(compressed_file_offsets_[i-1] + compressed_sizes_[i-1]);
    if (compressed_sizes_[i] > max_compressed_size_) {
      max_compressed_size_ = compressed_sizes_[i];
    }
  }
}

void SyncCommand::Impl::readMetadata()
{
  auto metadataReader = Reader::create(metadataUri);

  baseImpl.progressPhase++;
  baseImpl.progressTotalBytes = metadataReader->size();
  baseImpl.progressCurrentBytes = 0;
  baseImpl.progress_compressed_bytes_ = 0;

  auto &offset = baseImpl.progressCurrentBytes;

  parseHeader(*metadataReader);
  offset = headerSize;

  // NOTE: The current logic reads all metadata information regardless of whether it is actually used
  // Furthermore, it reads this information up front. This can potentially be optimized so as to
  // read only when reconstructing source chunk
  offset += ReadMetadataIntoContainer(*metadataReader.get(), offset, weakChecksums);
  offset += ReadMetadataIntoContainer(*metadataReader.get(), offset, strongChecksums);
  offset += ReadMetadataIntoContainer(*metadataReader.get(), offset, compressed_sizes_);

  UpdateCompressedOffsetsAndMaxSize();

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

    baseImpl.progressCurrentBytes += block;
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
  baseImpl.progressPhase++;
  baseImpl.progressTotalBytes = seedDataSize;
  baseImpl.progressCurrentBytes = 0;
  baseImpl.progress_compressed_bytes_ = 0;

  parallelize(
      seedDataSize,
      block,
      block,
      threads_,
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

  auto smart_decompression_buffer = std::make_unique<char[]>(max_compressed_size_);
  auto decompression_buffer = smart_decompression_buffer.get();

  auto seedReader = Reader::create(seedUri);
  auto dataReader = Reader::create(dataUri);

  std::ofstream& output = output_streams_[id];
  output.seekp(begOffset);

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
      if (compression_diabled_) 
      {
        count = dataReader->read(buffer, i * block, block);	
        downloadedBytes += count;
      }
      else
      {
        auto size_to_read = compressed_sizes_[i];
        auto offset_to_read_from = compressed_file_offsets_[i];
        LOG_ASSERT(size_to_read <= max_compressed_size_);
        count = dataReader->read(decompression_buffer, offset_to_read_from, size_to_read);
        downloadedBytes += count;
        baseImpl.progress_compressed_bytes_ += count;
        auto const expected_size_after_decompression = ZSTD_getFrameContentSize(decompression_buffer, count);
        CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_ERROR) << "Offset starting " << offset_to_read_from << " not compressed by zstd!";
        CHECK(expected_size_after_decompression != ZSTD_CONTENTSIZE_UNKNOWN) << "Original size unknown when decompressing from offset " << offset_to_read_from;
        CHECK(expected_size_after_decompression <= block) << "Expected decompressed size is greater than block size. Starting offset " << offset_to_read_from;
        auto decompressed_size = ZSTD_decompress(buffer, block, decompression_buffer, count);
        CHECK(!ZSTD_isError(decompressed_size)) << ZSTD_getErrorName(decompressed_size);
        LOG_ASSERT(decompressed_size <= block);
        count = decompressed_size;
      }
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

    baseImpl.progressCurrentBytes += count;
  }
  // Ensure the file is flushed before additional checks such as hash 
  // comparisons are performed. The checks can happen any time after this 
  // function completes (for all threads)
  output.flush();
}

// Reserve file streams/handles for each potential thread. Calling 
//   std::ofstream output(outputPath, std::ios::binary)
// in each thread can lead to race conditions. Specifically, the above call
// can cause the output file to be reinitialized (despite lack of the 
// truncate flag) overwriting information written at start by another thread.
void SyncCommand::Impl::ReserveFileStreams()
{
  for (int id = 0; id < threads_; id++)
  {
    // NOTE: Reservation is done for all potential threads even if later fewer
    // threads are created. A complexity/optimization trade-off is to identify
    // final number of threads and then to create these.
    output_streams_.push_back(std::move(std::ofstream(outputPath, std::ios::binary)));
    output_streams_[id].exceptions(std::ofstream::failbit | std::ofstream::badbit);
  }
}

void SyncCommand::Impl::reconstructSource()
{
  auto dataReader = Reader::create(dataUri);
  auto dataSize = size;

  baseImpl.progressPhase++;
  baseImpl.progressTotalBytes = dataSize;
  baseImpl.progressCurrentBytes = 0;
  baseImpl.progress_compressed_bytes_ = 0;

  ReserveFileStreams();
  // NOTE: seekp() is expected to automatically extend the file.
  // The below is added more as a precaution to prevent a race where we have 
  // seekp-extend in one thread while another thread is flushing its buffer.
  std::filesystem::resize_file(outputPath, dataSize);

  auto actualThreads = parallelize(
      dataSize,
      block,
      0,
      threads_,
      [this](auto id, auto beg, auto end) {
        reconstructSourceChunk(id, beg, end);
      });

  baseImpl.progressPhase++;
  baseImpl.progressTotalBytes = dataSize;
  baseImpl.progressCurrentBytes = 0;
  // NOTE: Compressed bytes is consciously not set to 0 to preserve compressed
  // bytes information from the last phase as the final reconstruction from
  // source does not change compressed bytes read. downloadedBytes follows a 
  // similar pattern.

  constexpr auto bufferSize = 1024 * 1024;
  auto smartBuffer = std::make_unique<char[]>(bufferSize);
  auto buffer = smartBuffer.get();

  StrongChecksumBuilder outputHash;

  std::ifstream read_for_hash_check(outputPath, std::ios::binary);
  while (read_for_hash_check) {
    auto count = read_for_hash_check.read(buffer, bufferSize).gcount();
    outputHash.update(buffer, count);
    baseImpl.progressCurrentBytes += count;
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
  VISIT(visitor, weakChecksumMatches);
  VISIT(visitor, weakChecksumFalsePositive);
  VISIT(visitor, strongChecksumMatches);
  VISIT(visitor, reusedBytes);
  VISIT(visitor, downloadedBytes);
}

SyncCommand::SyncCommand(
    std::string dataUri,
    bool compression_diabled,
    std::string metadataUri,
    std::string seedUri,
    std::filesystem::path outputPath,
    int threads)
    : pImpl(new Impl(
          std::move(dataUri),
          compression_diabled,
          std::move(metadataUri),
          std::move(seedUri),
          std::move(outputPath),
          threads,
          *Command::pImpl))
{
}

SyncCommand::~SyncCommand() = default;

int SyncCommand::run()
{
  return pImpl->run();
}

void SyncCommand::accept(MetricVisitor &visitor) const
{
  Command::accept(visitor);
  pImpl->accept(visitor, *this);
}
