#include <glog/logging.h>
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

  WeakChecksumCallback check = [&count](auto data, auto size, auto wcs) {
    auto simple_wcs = weakChecksum(data, size);
    EXPECT_EQ(wcs, simple_wcs);
    count += 1;
  };

  auto cs = weakChecksum(data + size, size, 0, check, true);
  EXPECT_EQ(count, 0);
  EXPECT_EQ(cs, 183829005);

  cs = weakChecksum(data + 2 * size, size, cs, check);
  EXPECT_EQ(count, 10);
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

  static void readMetadata(const SyncCommand &c) {
    c.pImpl->readMetadata();
  }
};

PrepareCommand prepare(std::string data, const fs::path &metadataPath)
{
  const size_t size = data.size();
  auto reader = MemoryReader(data.data(), size);

  constexpr size_t block = 4;
  const size_t blockCount = (size + block - 1) / block;

  // FIXME: figure out how to make this close automatic... / destructor based
  std::ofstream output(metadataPath, std::ios::binary);
  PrepareCommand c(reader, block, output);
  c.run();
  output.close();

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

  // FIXME: maybe make the expectation checking in a method??
  ExpectationCheckMetricVisitor(  // NOLINT(bugprone-unused-raii)
      c,
      {
          {"//progressCurrentBytes", 12},
          {"//progressTotalBytes", 10},
          {"//reader/totalBytesRead", 10},
          {"//reader/totalReads", 3},
      });

  return std::move(c);
}

TEST(PrepareCommand, Simple)
{
  auto data = "0123456789";
  auto metadataPath = fs::temp_directory_path() / "output.ksync";
  prepare(data, metadataPath);
}

TEST(SyncCommand, MetadataRoundtrip)
{
  std::string data = "0123456789";
  auto metadataPath = fs::temp_directory_path() / "output.ksync";
  auto pc = prepare(data, metadataPath);

  // TODO: weird C++ gotcha to figure out...
  //  when we don't create the readers outside as separate variables, haywire!!!
  auto metadataReader = FileReader(metadataPath);
  auto dataReader = MemoryReader(data.data(), data.size());
  auto sc = SyncCommand(metadataReader, dataReader);

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

TEST(SyncCommand, Simple)
{
  std::string data = "0123456789";
  auto metadataPath = fs::temp_directory_path() / "output.ksync";
  auto pc = prepare(data, metadataPath);

  auto metadataReader = FileReader(metadataPath);
  auto dataReader = MemoryReader(data.data(), data.size());
  auto sc = SyncCommand(metadataReader, dataReader);

  sc.run();
}