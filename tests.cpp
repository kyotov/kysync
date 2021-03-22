#include <glog/logging.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "checksums/StrongChecksum.h"
#include "checksums/wcs.h"
#include "commands/PrepareCommand.h"
#include "commands/SyncCommand.h"
#include "commands/impl/PrepareCommandImpl.h"
#include "commands/impl/SyncCommandImpl.h"
#include "metrics/ExpectationCheckMetricsVisitor.h"
#include "readers/FileReader.h"
#include "readers/MemoryReader.h"
#include "Config.h"

namespace fs = std::filesystem;

TEST(WeakChecksum, Simple)
{
  auto data = "0123456789";
  auto wcs = weakChecksum(data, strlen(data));
  EXPECT_EQ(wcs, 183829005);
}

TEST(WeakChecksum, Rolling)
{
  char data[] = "012345678901234567890123456789";
  ASSERT_EQ(strlen(data), 30);

  auto size = strlen(data) / 3;
  memset(data, 0, size);

  std::atomic<int> count = 0;

  int64_t warmup = size - 1;

  auto check = [&](auto offset, auto wcs) {
    if (--warmup < 0) {
      auto simple_wcs = weakChecksum(data + 2 * size + offset, size);
      EXPECT_EQ(wcs, simple_wcs);
      count += 1;
    }
  };

  auto cs = weakChecksum(data + size, size, 0, check);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(cs, 183829005);

  cs = weakChecksum(data + 2 * size, size, cs, check);
  EXPECT_EQ(count, 11);
  EXPECT_EQ(cs, 183829005);
}

TEST(StringChecksum, Simple)
{
  auto data = "0123456789";
  auto scs = StrongChecksum::compute(data, strlen(data));
  EXPECT_EQ(scs.toString(), "e353667619ec664b49655fc9692165fb");
}

TEST(StringChecksum, Stream)
{
  constexpr int COUNT = 10'000;
  std::stringstream s;

  auto data = "0123456789";

  for (int i = 0; i < COUNT; i++) {
    s.write(data, strlen(data));
  }

  auto str = s.str();
  ASSERT_EQ(str.length(), COUNT * strlen(data));

  auto buffer = str.c_str();

  auto scs1 = StrongChecksum::compute(buffer, COUNT * strlen(data));

  s.seekg(0);
  auto scs2 = StrongChecksum::compute(s);

  EXPECT_EQ(scs1, scs2);
}

void testReader(Reader &reader, size_t expectedSize)
{
  ASSERT_EQ(reader.size(), expectedSize);

  char buffer[1024];

  {
    auto count = reader.read(buffer, 1, 3);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer, "123");
  }

  {
    auto count = reader.read(buffer, 8, 3);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer, "89");
  }

  {
    auto count = reader.read(buffer, 20, 5);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer, "");
  }

  ExpectationCheckMetricVisitor(
      reader,
      {//
       {"//totalReads", 3},
       {"//totalBytesRead", 5}});
}

std::string createMemoryReaderUri(const void *address, size_t size)
{
  std::stringstream result;
  result << "memory://" << address << ":" << std::hex << size;
  return result.str();
}

std::string createMemoryReaderUri(const std::string &data)
{
  return createMemoryReaderUri(data.data(), data.size());
}

TEST(Readers, MemoryReaderSimple)
{
  auto data = "0123456789";

  auto reader = MemoryReader(data, strlen(data));
  testReader(reader, strlen(data));

  auto uri = createMemoryReaderUri(data, strlen(data));
  testReader(*Reader::create(uri), strlen(data));
}

TEST(Readers, FileReaderSimple)
{
  auto data = "0123456789";

  auto path = fs::temp_directory_path() / "kysync.testdata";
  std::ofstream f(path, std::ios::binary);
  f.write(data, strlen(data));
  f.close();

  auto reader = FileReader(path);
  testReader(reader, fs::file_size(path));

  testReader(*Reader::create("file://" + path.string()), strlen(data));
}

TEST(Readers, BadUri)
{
  // invalid protocol
  EXPECT_THROW(Reader::create("foo://1234"), std::invalid_argument);

  // invalid buffer
  EXPECT_THROW(Reader::create("memory://G:0"), std::invalid_argument);
  // invalid size
  EXPECT_THROW(Reader::create("memory://0:G"), std::invalid_argument);
  // invalid separator
  EXPECT_THROW(Reader::create("memory://0+0"), std::invalid_argument);

  // non-existent file
  EXPECT_THROW(Reader::create("file://foo"), std::invalid_argument);
}

class KySyncTest {
public:
  static const std::vector<uint32_t> &examineWeakChecksums(
      const PrepareCommand &c)
  {
    return c.pImpl->weakChecksums;
  }

  static const std::vector<StrongChecksum> &examineStrongChecksums(
      const PrepareCommand &c)
  {
    return c.pImpl->strongChecksums;
  }

  static const std::vector<uint32_t> &examineWeakChecksums(const SyncCommand &c)
  {
    return c.pImpl->weakChecksums;
  }

  static const std::vector<StrongChecksum> &examineStrongChecksums(
      const SyncCommand &c)
  {
    return c.pImpl->strongChecksums;
  }

  static void readMetadata(const SyncCommand &c)
  {
    c.pImpl->readMetadata();
  }

  static std::vector<size_t> examineAnalisys(const SyncCommand &c)
  {
    std::vector<size_t> result;

    for (size_t i = 0; i < c.pImpl->blockCount; i++) {
      auto wcs = c.pImpl->weakChecksums[i];
      auto data = c.pImpl->analysis[wcs];
      result.push_back(data.seedOffset);
    }

    return result;
  }
};

std::stringstream createInputStream(const std::string &data)
{
  std::stringstream result;
  result.write(data.data(), data.size());
  result.seekg(0);
  return result;
}

PrepareCommand prepare(const std::string& data, std::ostream &output_ksync, std::ostream &output_compressed, size_t block)
{
  const auto size = data.size();

  auto input = createInputStream(data);
  PrepareCommand c(input, output_ksync, output_compressed, block);
  c.run();

  // FIXME: maybe make the expectation checking in a method??
  ExpectationCheckMetricVisitor(  // NOLINT(bugprone-unused-raii)
      c,
      {
          {"//progressCurrentBytes", size},
          {"//progressTotalBytes", size},
      });

  return std::move(c);
}

TEST(PrepareCommand, Simple)
{
  auto data = std::string("0123456789");
  auto block = 4;
  auto blockCount = (data.size() + block - 1) / block;

  std::stringstream output_ksync;
  std::stringstream output_compressed;
  auto c = prepare(data, output_ksync, output_compressed, block);

  const auto &wcs = KySyncTest::examineWeakChecksums(c);
  EXPECT_EQ(wcs.size(), blockCount);
  EXPECT_EQ(weakChecksum("0123", block), wcs[0]);
  EXPECT_EQ(weakChecksum("4567", block), wcs[1]);
  EXPECT_EQ(weakChecksum("89\0\0", block), wcs[2]);

  const auto &scs = KySyncTest::examineStrongChecksums(c);
  EXPECT_EQ(wcs.size(), blockCount);
  EXPECT_EQ(StrongChecksum::compute("0123", block), scs[0]);
  EXPECT_EQ(StrongChecksum::compute("4567", block), scs[1]);
  EXPECT_EQ(StrongChecksum::compute("89\0\0", block), scs[2]);
}

TEST(PrepareCommand, Simple2)
{
  auto data = std::string("123412341234");
  auto block = 4;
  auto blockCount = (data.size() + block - 1) / block;

  std::stringstream output_ksync;
  std::stringstream output_compressed;
  auto c = prepare(data, output_ksync, output_compressed, block);

  const auto &wcs = KySyncTest::examineWeakChecksums(c);
  EXPECT_EQ(wcs.size(), blockCount);
  EXPECT_EQ(weakChecksum("1234", block), wcs[0]);
  EXPECT_EQ(weakChecksum("1234", block), wcs[1]);
  EXPECT_EQ(weakChecksum("1234", block), wcs[2]);

  const auto &scs = KySyncTest::examineStrongChecksums(c);
  EXPECT_EQ(wcs.size(), blockCount);
  EXPECT_EQ(StrongChecksum::compute("1234", block), scs[0]);
  EXPECT_EQ(StrongChecksum::compute("1234", block), scs[1]);
  EXPECT_EQ(StrongChecksum::compute("1234", block), scs[2]);
}

TEST(SyncCommand, MetadataRoundtrip)
{
  auto block = 4;
  std::string data = "0123456789";
  std::stringstream metadata;
  std::stringstream compressed;
  auto pc = prepare(data, metadata, compressed, block);

  auto input = createMemoryReaderUri(data);
  // std::stringstream output;
  auto outputPath = fs::temp_directory_path() / "kysync.temp";

  auto metadataStr = metadata.str();

  auto sc = SyncCommand(
      createMemoryReaderUri(data),
      createMemoryReaderUri(metadataStr),
      input,
      outputPath,
      1);

  KySyncTest::readMetadata(sc);

  // expect that the checksums had a successful round trip
  EXPECT_EQ(
      KySyncTest::examineWeakChecksums(pc),
      KySyncTest::examineWeakChecksums(sc));
  EXPECT_EQ(
      KySyncTest::examineStrongChecksums(pc),
      KySyncTest::examineStrongChecksums(sc));

  ExpectationCheckMetricVisitor(  // NOLINT(bugprone-unused-raii)
      sc,
      {
          {"//progressPhase", 1},
          {"//progressTotalBytes", 159},
          {"//progressCurrentBytes", 159},
      });
}

void EndToEndTest(
    const std::string &sourceData,
    const std::string &seedData,
    size_t block,
    const std::vector<size_t> &expectedBlockMapping)
{
  LOG(INFO) << "E2E for " << sourceData.substr(0, 40);

  std::stringstream metadata;
  std::stringstream compressed;
  auto pc = prepare(sourceData, metadata, compressed, block);

  auto input = createMemoryReaderUri(seedData);
  // std::stringstream output;
  // std::ofstream output(fs::temp_directory_path() / "t", std::ios::binary);
  auto outputPath = fs::temp_directory_path() / "kysync.temp";
  if (fs::exists(outputPath)) {
    fs::remove(outputPath);
  }

  auto metadataStr = metadata.str();
  auto compressedStr = compressed.str();

  auto sc = SyncCommand(
      createMemoryReaderUri(compressedStr),
      createMemoryReaderUri(metadataStr),
      input,
      outputPath,
      1);

  sc.run();

  EXPECT_EQ(KySyncTest::examineAnalisys(sc), expectedBlockMapping);

  size_t outputSize = fs::file_size(outputPath);
  EXPECT_EQ(sourceData.size(), outputSize);

  auto output = std::ifstream(outputPath, std::ios::binary);
  auto buffer = std::make_unique<char[]>(outputSize + 1);
  output.read(buffer.get(), outputSize);
  buffer.get()[outputSize] = '\0';

  EXPECT_EQ(sourceData, buffer.get());
}

TEST(SyncCommand, EndToEnd)
{
  // FIXME: research if we can do parametrized testing...
  std::string data = "0123456789";
  EndToEndTest(data, data, 4, {0, 4, -1ull /*8*/});
  EndToEndTest(data, data, 6, {0, -1ull /*6*/});

  EndToEndTest("0123456789", "001234004567", 4, {1, 8, -1ull});
  EndToEndTest("123412341234", "00123400", 4, {2, 2, 2});
  EndToEndTest("12345678", "", 4, {-1ull, -1ull});
  EndToEndTest(
      "abcdefjhijklmnopqrstuvwxyz",
      "_qrst_mnop_ijkl_abcd_efjh_uvwx_yz",
      4,
      {16, 21, 11, 6, 1, 26, -1ull /*31*/});

  if (WARMUP_AFTER_MATCH) {
    EndToEndTest(
        "1234234534564567567867897890",
        "1234567890",
        4,
        {0, -1ull, -1ull, -1ull, 4, -1ull, -1ull});
  } else {
    EndToEndTest(
        "1234234534564567567867897890",
        "1234567890",
        4,
        {0, 1, 2, 3, 4, 5, 6});
  }

  std::string chunk = "1234";
  std::stringstream input;
  std::vector<size_t> expected;
  for (auto i = 0; i < 1024; i++) {
    input << chunk;
    expected.push_back(0);
  }
  auto inputData = input.str();
  EndToEndTest(inputData, "1234", 4, expected);
}

// NOTE: This attempts to use the new style despite inconsistency
int PrepareFile(const std::string &source_file_name, size_t block_size, const std::string &output_metadata_file_name, const std::string &output_compressed_file_name)
{
  auto output_metadata = std::ofstream(output_metadata_file_name, std::ios::binary);
  CHECK(output_metadata) << "unable to write to " << output_metadata_file_name;
  auto output_compressed = std::ofstream(output_compressed_file_name, std::ios::binary);
  CHECK(output_compressed) << "unable to write to " << output_compressed_file_name;
  auto input = std::ifstream(source_file_name, std::ios::binary);
  CHECK(input) << "unable to read from " << source_file_name;
  return PrepareCommand(input, output_metadata, output_compressed, block_size).run();
}

bool DoFilesMatch(const std::string &first_file_name, const std::string &second_file_name) 
{
  if (std::filesystem::file_size(first_file_name) != std::filesystem::file_size(second_file_name)) {
    return false;
  }
    
  std::ifstream first(first_file_name, std::ifstream::binary);
  CHECK(first) << "Could not open " << first_file_name;

  std::ifstream second(second_file_name, std::ifstream::binary);
  CHECK(second) << "Could not open " << second_file_name;

  LOG_ASSERT(first.tellg() == 0) << "Unexpected file position on open";
  LOG_ASSERT(second.tellg() == 0) << "Unexpected file position on open";
  return std::equal(
    std::istreambuf_iterator<char>(first.rdbuf()),
    std::istreambuf_iterator<char>(),
    std::istreambuf_iterator<char>(second.rdbuf()));
}

void SyncFromCompressedFile(
    const std::string &compressed_file_name,
    const std::string &metadata_file_name,    
    const std::string &seed_data_file_name,
    size_t block_size,
    const std::string &temp_path_name,
    const std::string &expected_output_file_name)
{
  std::string file_uri_prefix = "file://";
  std::string output_file_name = temp_path_name + "/syncd_output_file";
  auto sync_command = SyncCommand(
    file_uri_prefix + compressed_file_name,
    file_uri_prefix + metadata_file_name,
    file_uri_prefix + seed_data_file_name,
    output_file_name,
    32);
  auto return_code = sync_command.run();
  CHECK(return_code == 0) << "Sync command failed for " + compressed_file_name;
  EXPECT_TRUE(DoFilesMatch(expected_output_file_name, output_file_name)) << "Sync'd output file does not match expectations";
}

std::filesystem::path CreateAndGetTempDir()
{
  // NOTE: We can use a temporary/random dirname to enable multiple test runs
  // on the same machine. We currently use the same name mostly for convenience.
  std::filesystem::path temp_path = fs::temp_directory_path() / "ksync_files_test";
  if (fs::exists(temp_path)) 
  {
    fs::remove_all(temp_path);
  }
  fs::create_directories(temp_path);
  return std::move(temp_path);
}

void ClearTempDir(const std::filesystem::path &temp_path)
{
  fs::remove_all(temp_path);
}

void EndToEndFilesTest(
    const std::string &source_file_name,
    const std::string &seed_data_file_name,
    size_t block_size,
    const std::string &expected_metadata_file_name,
    const std::string &expected_compressed_file_name,
    const std::string &expected_output_file_name)
{
  LOG(INFO) << "E2E Files Test for " << source_file_name;
  LOG(INFO) << "Using seed file " << seed_data_file_name;

  std::filesystem::path temp_path = CreateAndGetTempDir();
  std::string temp_path_name = temp_path.string();
  std::string metadata_file_name = temp_path_name + "/i.ksync";
  std::string compressed_file_name = temp_path_name + "/i.pzst";
  auto return_code = PrepareFile(
    source_file_name, 
    block_size,
    metadata_file_name,
    compressed_file_name);
  CHECK(return_code == 0) << "Prepare command failed for " + source_file_name;
  EXPECT_TRUE(DoFilesMatch(expected_metadata_file_name, metadata_file_name)) << "Generated metadata files does not match expectations";
  EXPECT_TRUE(DoFilesMatch(expected_compressed_file_name, compressed_file_name)) << "Generated compressed file does not match expectations";
  EXPECT_FALSE(DoFilesMatch(expected_metadata_file_name, compressed_file_name)) << "Unexpected match of metadata and compressed files";

  SyncFromCompressedFile(
    compressed_file_name,
    metadata_file_name,
    source_file_name,
    block_size,
    temp_path_name,
    expected_output_file_name);

  ClearTempDir(temp_path);
}

void RunEndToEndFilesTestFor(const std::string &file_name)
{
  EndToEndFilesTest(
    file_name, 
    file_name,
    1024,
    file_name + ".ksync",
    file_name + ".pzst",
    file_name);
}

TEST(SyncCommand, EndToEndFiles)
{
  // NOTE: This currently assumes that a test data dir exists one 
  // level above the test's working directory
  std::string test_data_path = "../test_data";
  RunEndToEndFilesTestFor(test_data_path + "/test_file_small.txt");
  RunEndToEndFilesTestFor(test_data_path + "/test_file.txt");
}

TEST(SyncCommand, SyncFileFromSeed)
{
  // NOTE: This currently assumes that a test data dir exists one 
  // level above the test's working directory
  std::string test_data_path = "../test_data";
  std::string sync_file_name = test_data_path + "/test_file_v2.txt";
  std::string seed_file_name = test_data_path + "/test_file.txt";
  std::filesystem::path temp_path = CreateAndGetTempDir();
  SyncFromCompressedFile(
    sync_file_name + ".pzst",
    sync_file_name + ".ksync",
    seed_file_name,
    1024,
    temp_path.string(),
    sync_file_name);  
  ClearTempDir(temp_path);
}