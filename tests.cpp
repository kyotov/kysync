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

  // TODO: A move instead of a copy causes a seg fault. To be investigated and fixed
  return c; // std::move(c);
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
          {"//progressTotalBytes", 135},
          {"//progressCurrentBytes", 135},
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

  auto sc = SyncCommand(
      createMemoryReaderUri(sourceData),
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
