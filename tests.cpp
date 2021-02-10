#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>

#include "checksums/scs.h"
#include "checksums/wcs.h"
#include "commands/PrepareCommand.h"
#include "commands/SyncCommand.h"
#include "commands/impl/PrepareCommandImpl.h"
#include "commands/impl/SyncCommandImpl.h"
#include "metrics/ExpectationCheckMetricsVisitor.h"
#include "readers/FileReader.h"
#include "readers/MemoryReader.h"

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

  std::atomic<int> count{0};

  WeakChecksumCallback check = [&count, &data, &size](auto offset, auto wcs) {
    auto simple_wcs = weakChecksum(data + 2 * size + offset, size);
    EXPECT_EQ(wcs, simple_wcs);
    count += 1;
  };

  auto cs = weakChecksum(data + size, size, 0, check, true);
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

TEST(Readers, MemoryReaderSimple)
{
  auto data = "0123456789";

  auto reader = MemoryReader(data, strlen(data));
  testReader(reader, strlen(data));

  std::stringstream s;
  s << "memory://" << (void *)data << ":" << std::hex << strlen(data);

  testReader(*Reader::create(s.str()), strlen(data));
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

PrepareCommand
prepare(std::string data, const fs::path &metadataPath, size_t block)
{
  const size_t size = data.size();
  auto reader = MemoryReader(data.data(), size);

  const size_t blockCount = (size + block - 1) / block;

  // FIXME: figure out how to make this close automatic... / destructor based
  std::ofstream output(metadataPath, std::ios::binary);
  PrepareCommand c(reader, block, output);
  c.run();
  output.close();

  // FIXME: maybe make the expectation checking in a method??
  ExpectationCheckMetricVisitor(  // NOLINT(bugprone-unused-raii)
      c,
      {
          {"//progressCurrentBytes", blockCount * block},
          {"//progressTotalBytes", size},
          {"//reader/totalBytesRead", size},
          {"//reader/totalReads", blockCount},
      });

  return std::move(c);
}

TEST(PrepareCommand, Simple)
{
  auto data = std::string("0123456789");
  auto block = 4;
  auto blockCount = (data.size() + block - 1) / block;
  auto metadataPath = fs::temp_directory_path() / "output.ksync";
  auto c = prepare(data, metadataPath, block);

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

TEST(SyncCommand, MetadataRoundtrip)
{
  auto block = 4;
  std::string data = "0123456789";
  auto metadataPath = fs::temp_directory_path() / "output.ksync";
  auto pc = prepare(data, metadataPath, block);

  // TODO: weird C++ gotcha to figure out...
  //  when we don't create the readers outside as separate variables, haywire!!!
  auto metadataReader = FileReader(metadataPath);
  auto dataReader = MemoryReader(data.data(), data.size());
  auto outputPath = fs::temp_directory_path() / "output";
  auto sc = SyncCommand(metadataReader, dataReader, dataReader, outputPath);

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
          {"//dataReader/totalBytesRead", 0},
          {"//dataReader/totalReads", 0},
          {"//metadataReader/totalBytesRead", 156},
          {"//metadataReader/totalReads", 3},
      });
}

void EndToEndTest(
    const std::string &sourceData,
    const std::string &seedData,
    size_t block,
    const std::vector<size_t> &expectedBlockMapping)
{
  auto metadataPath = fs::temp_directory_path() / "output.ksync";
  auto pc = prepare(sourceData, metadataPath, block);

  auto metadataReader = FileReader(metadataPath);
  auto dataReader = MemoryReader(sourceData.data(), sourceData.size());
  auto seedReader = MemoryReader(seedData.data(), seedData.size());
  auto outputPath = fs::temp_directory_path() / "output";
  auto sc = SyncCommand(metadataReader, dataReader, seedReader, outputPath);

  sc.run();

  EXPECT_EQ(KySyncTest::examineAnalisys(sc), expectedBlockMapping);

  auto outputSize = fs::file_size(outputPath);
  auto buffer = std::make_unique<char[]>(outputSize + 1);
  auto output = std::ifstream(outputPath, std::ios::binary);
  output.read(buffer.get(), outputSize);
  buffer[outputSize] = 0;

  EXPECT_EQ(sourceData, buffer.get());
}

TEST(SyncCommand, EndToEnd)
{
  // FIXME: research if we can do parametrized testing...
  std::string data = "0123456789";
  EndToEndTest(data, data, 4, {0, 4, 8});
  EndToEndTest(data, data, 6, {0, 6});

  EndToEndTest("0123456789", "001234004567", 4, {1, 8, -1ull});
  EndToEndTest("123412341234", "00123400", 4, {2, 2, 2});
  EndToEndTest("12345678", "", 4, {-1ull, -1ull});
  EndToEndTest(
      "abcdefjhijklmnopqrstuvwxyz",
      "_qrst_mnop_ijkl_abcd_efjh_uvwx_yz",
      4,
      {16, 21, 11, 6, 1, 26, 31});
  EndToEndTest(
      "1234234534564567567867897890",
      "1234567890",
      4,
      {0, 1, 2, 3, 4, 5, 6});
}
