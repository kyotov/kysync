#include <glog/logging.h>
#include <gtest/gtest.h>
#include <httplib.h>
#include <ky/temp_path.h>
#include <kysync/checksums/strong_checksum.h>
#include <kysync/checksums/weak_checksum.h>
#include <kysync/commands/prepare_command.h>
#include <kysync/commands/sync_command.h>
#include <kysync/path_config.h>
#include <kysync/readers/file_reader.h>
#include <kysync/readers/memory_reader.h>
#include <kysync/test_common/expectation_check_metrics_visitor.h>
#include <kysync/test_common/test_environment.h>
#include <kysync/test_common/test_fixture.h>
#include <zstd.h>

#include <filesystem>
#include <fstream>

//#include <boost/asio/post.hpp>
//#include <boost/asio/thread_pool.hpp>

namespace kysync {

namespace fs = std::filesystem;

std::streamsize Size(const char *data) {
  // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
  return strlen(data);
}

std::streamsize Size(char *data) {
  // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
  return strlen(data);
}

std::streamsize Size(const fs::path &path) {
  // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
  return fs::file_size(path);
}

template <typename T>
std::streamsize Size(const T &v) {
  return v.size();
}

class Tests : public Fixture {
protected:
  static std::string GetTestDataPath() {
    return TestEnvironment::GetEnv(
        "TEST_DATA_DIR",
        (CMAKE_SOURCE_DIR / "test_data").string());
  }

  static constexpr int kThreads = 32;
};

TEST_F(Tests, SimpleWeakChecksum) {  // NOLINT
  const auto *data = "0123456789";
  auto wcs = WeakChecksum(data, Size(data));
  EXPECT_EQ(wcs, 183829005);
}

TEST_F(Tests, RollingWeakChecksum) {  // NOLINT
  std::string s = "012345678901234567890123456789";
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  char *data = s.data();
  ASSERT_EQ(s.size(), 30);

  std::streamsize size = Size(data) / 3;
  memset(data, 0, size);

  std::atomic<int> count = 0;

  int64_t warmup = size - 1;

  auto check = [&](auto offset, auto wcs) {
    if (--warmup < 0) {
      auto simple_wcs = WeakChecksum(data + 2 * size + offset, size);
      EXPECT_EQ(wcs, simple_wcs);
      count += 1;
    }
  };

  auto cs = WeakChecksum(data + size, size, 0, check);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(cs, 183829005);

  cs = WeakChecksum(data + 2 * size, size, cs, check);
  EXPECT_EQ(count, 11);
  EXPECT_EQ(cs, 183829005);
}

TEST_F(Tests, SimpleStringChecksum) {  // NOLINT
  const auto *data = "0123456789";
  auto scs = StrongChecksum::Compute(data, Size(data));
  EXPECT_EQ(scs.ToString(), "e353667619ec664b49655fc9692165fb");
}

TEST_F(Tests, StreamingStringChecksum) {  // NOLINT
  static constexpr int kCount = 10'000;
  std::stringstream s;

  const auto *data = "0123456789";

  for (int i = 0; i < kCount; i++) {
    s.write(data, Size(data));
  }

  auto str = s.str();
  ASSERT_EQ(str.length(), kCount * Size(data));

  const auto *buffer = str.c_str();

  auto scs_1 = StrongChecksum::Compute(buffer, kCount * Size(data));

  s.seekg(0);
  auto scs_2 = StrongChecksum::Compute(s);

  EXPECT_EQ(scs_1, scs_2);
}

void TestReader(Reader &reader, std::streamsize expected_size) {
  ASSERT_EQ(reader.GetSize(), expected_size);

  std::array<char, 1024> buffer{};

  {
    auto count = reader.Read(buffer.data(), 1, 3);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer.data(), "123");
  }

  {
    auto count = reader.Read(buffer.data(), 8, 3);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer.data(), "89");
  }

  {
    auto count = reader.Read(buffer.data(), 20, 5);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer.data(), "");
  }

  ExpectationCheckMetricVisitor(
      reader,
      {//
       {"//total_reads_", 3},
       {"//total_bytes_read_", 5}});
}

std::string CreateMemoryReaderUri(const void *address, std::streamsize size) {
  std::stringstream result;
  result << "memory://" << address << ":" << std::hex << size;
  return result.str();
}

std::string CreateMemoryReaderUri(const std::string &data) {
  return CreateMemoryReaderUri(data.data(), Size(data));
}

TEST_F(Tests, MemoryReaderSimple) {  // NOLINT
  const auto *data = "0123456789";

  auto reader = MemoryReader(data, Size(data));
  TestReader(reader, Size(data));

  auto uri = CreateMemoryReaderUri(data, Size(data));
  TestReader(*Reader::Create(uri), Size(data));
}

TEST_F(Tests, FileReaderSimple) {  // NOLINT
  const auto *data = "0123456789";

  auto tmp = ky::TempPath();
  auto path = tmp.GetPath() / "data.bin";

  std::ofstream f(path, std::ios::binary);
  f.write(data, Size(data));
  f.close();

  auto reader = FileReader(path);
  TestReader(reader, Size(path));
  TestReader(*Reader::Create("file://" + path.string()), Size(data));
}

TEST_F(Tests, ReadersWithBadUri) {  // NOLINT
  // invalid protocol
  EXPECT_THROW(  // NOLINT{cppcoreguidelines-avoid-goto}
      Reader::Create("foo://1234"),
      std::invalid_argument);

  // invalid buffer
  EXPECT_THROW(  // NOLINT{cppcoreguidelines-avoid-goto}
      Reader::Create("memory://G:0"),
      std::invalid_argument);

  // invalid size
  EXPECT_THROW(  // NOLINT{cppcoreguidelines-avoid-goto}
      Reader::Create("memory://0:G"),
      std::invalid_argument);

  // invalid separator
  EXPECT_THROW(  // NOLINT{cppcoreguidelines-avoid-goto}
      Reader::Create("memory://0+0"),
      std::invalid_argument);

  // non-existent file
  EXPECT_THROW(  // NOLINT{cppcoreguidelines-avoid-goto}
      Reader::Create("file://foo"),
      std::invalid_argument);
}

void WriteFile(const fs::path &path, const std::string &content) {
  auto f = std::ofstream(path, std::ios::binary);
  f.write(content.data(), Size(content));
}

std::string ReadFile(const fs::path &path) {
  auto result = std::string();
  result.resize(fs::file_size(path));

  auto f = std::ifstream(path, std::ios::binary);
  f.read(result.data(), Size(result));

  return result;
}

class KySyncTest {
public:
  static const std::vector<uint32_t> &ExamineWeakChecksums(
      const KySyncCommand &c) {
    return c.GetWeakChecksums();
  }

  static const std::vector<StrongChecksum> &ExamineStrongChecksums(
      const KySyncCommand &c) {
    return c.GetStrongChecksums();
  }

  static void ReadMetadata(SyncCommand &c) { c.ReadMetadata(); }

  static auto ExamineAnalisys(SyncCommand &c) { return c.GetTestAnalysis(); }
};

std::stringstream CreateInputStream(const std::string &data) {
  std::stringstream result;
  result.write(data.data(), Size(data));
  result.seekg(0);
  return result;
}

std::streamsize GetExpectedCompressedSize(
    const std::string &data,
    int compression_level,
    std::streamsize block_size) {
  auto compression_buffer_size = ZSTD_compressBound(block_size);
  auto compression_buffer = std::vector<char>(compression_buffer_size);
  std::streamsize compressed_size = 0;
  std::streamsize size_read = 0;
  while (size_read < data.size()) {
    auto size_remaining = Size(data) - size_read;
    std::streamsize size_to_read = std::min(block_size, size_remaining);
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
    compressed_size += ZSTD_compress(
        compression_buffer.data(),
        compression_buffer_size,
        data.data() + size_read,
        size_to_read,
        compression_level);
    CHECK(!ZSTD_isError(compressed_size)) << ZSTD_getErrorName(compressed_size);
    size_read += size_to_read;
  }
  return compressed_size;
}

TEST_F(Tests, SimplePrepareCommand) {  // NOLINT
  auto data = std::string("0123456789");
  auto block = 4;
  auto block_count = (data.size() + block - 1) / block;

  std::stringstream output_ksync;
  std::stringstream output_compressed;

  auto tmp = ky::TempPath();
  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";

  WriteFile(data_path, data);
  auto c = PrepareCommand::Create(data_path, kysync_path, pzst_path, block, 1);
  c->Run();

  const auto &wcs = KySyncTest::ExamineWeakChecksums(*c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(WeakChecksum("0123", block), wcs[0]);
  EXPECT_EQ(WeakChecksum("4567", block), wcs[1]);
  EXPECT_EQ(WeakChecksum("89\0\0", block), wcs[2]);

  const auto &scs = KySyncTest::ExamineStrongChecksums(*c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(StrongChecksum::Compute("0123", block), scs[0]);
  EXPECT_EQ(StrongChecksum::Compute("4567", block), scs[1]);
  EXPECT_EQ(StrongChecksum::Compute("89\0\0", block), scs[2]);
}

TEST_F(Tests, SimplePrepareCommand2) {  // NOLINT
  auto data = std::string("123412341234");
  auto block = 4;
  auto block_count = (data.size() + block - 1) / block;

  auto tmp = ky::TempPath();
  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";

  WriteFile(data_path, data);
  auto c = PrepareCommand::Create(data_path, kysync_path, pzst_path, block, 1);
  c->Run();

  const auto &wcs = KySyncTest::ExamineWeakChecksums(*c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(WeakChecksum("1234", block), wcs[0]);
  EXPECT_EQ(WeakChecksum("1234", block), wcs[1]);
  EXPECT_EQ(WeakChecksum("1234", block), wcs[2]);

  const auto &scs = KySyncTest::ExamineStrongChecksums(*c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(StrongChecksum::Compute("1234", block), scs[0]);
  EXPECT_EQ(StrongChecksum::Compute("1234", block), scs[1]);
  EXPECT_EQ(StrongChecksum::Compute("1234", block), scs[2]);
}

TEST(Tests2, MetadataRoundtrip) {  // NOLINT
  auto block = 4;
  std::string data = "0123456789";

  auto tmp = ky::TempPath();
  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";
  auto output_path = tmp.GetPath() / "output.bin";

  WriteFile(data_path, data);
  auto pc = PrepareCommand::Create(data_path, kysync_path, pzst_path, block, 1);
  pc->Run();

  auto sc = SyncCommand::Create(
      "file://" + data_path.string(),
      "file://" + kysync_path.string(),
      "file://" + data_path.string(),
      output_path,
      true,
      4,
      1);

  KySyncTest::ReadMetadata(*sc);

  // expect that the checksums had a successful round trip
  EXPECT_EQ(
      KySyncTest::ExamineWeakChecksums(*pc),
      KySyncTest::ExamineWeakChecksums(*sc));
  EXPECT_EQ(
      KySyncTest::ExamineStrongChecksums(*pc),
      KySyncTest::ExamineStrongChecksums(*sc));
}

void EndToEndTest(
    const std::string &data,
    const std::string &seed_data,
    bool compression_disabled,
    std::streamsize block_size,
    const std::vector<std::streamoff>
        &expected_block_mapping) {  // NOLINT(misc-unused-parameters)
  LOG(INFO) << "E2E for " << data.substr(0, 40);

  auto tmp = ky::TempPath();

  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";
  auto seed_data_path = tmp.GetPath() / "seed_data.bin";
  auto output_path = tmp.GetPath() / "output.bin";

  WriteFile(data_path, data);
  WriteFile(seed_data_path, seed_data);
  PrepareCommand::Create(data_path, kysync_path, pzst_path, block_size, 1)
      ->Run();

  SyncCommand::Create(
      "file://" + (compression_disabled ? data_path : pzst_path).string(),
      "file://" + kysync_path.string(),
      "file://" + data_path.string(),
      output_path,
      compression_disabled,
      4,
      1)
      ->Run();

  EXPECT_EQ(data, ReadFile(output_path));
}

void RunEndToEndTests(bool compression_disabled) {
  // FIXME: research if we can do parametrized testing...
  std::string data = "0123456789";
  EndToEndTest(data, data, compression_disabled, 4, {0, 4, -1 /*8*/});
  EndToEndTest(data, data, compression_disabled, 6, {0, -1 /*6*/});

  EndToEndTest(
      "0123456789",
      "001234004567",
      compression_disabled,
      4,
      {1, 8, -1});
  EndToEndTest("123412341234", "00123400", compression_disabled, 4, {2, 2, 2});
  EndToEndTest("12345678", "", compression_disabled, 4, {-1, -1});
  EndToEndTest(
      "abcdefjhijklmnopqrstuvwxyz",
      "_qrst_mnop_ijkl_abcd_efjh_uvwx_yz",
      compression_disabled,
      4,
      {16, 21, 11, 6, 1, 26, -1 /*31*/});

  EndToEndTest(
      "1234234534564567567867897890",
      "1234567890",
      compression_disabled,
      4,
      {0, -1, -1, -1, 4, -1, -1});

  std::string chunk = "1234";
  std::stringstream input;
  std::vector<std::streamoff> expected;
  for (auto i = 0; i < 1024; i++) {
    input << chunk;
    expected.push_back(0);
  }
  auto input_data = input.str();
  EndToEndTest(input_data, "1234", compression_disabled, 4, expected);
}

TEST(SyncCommand, EndToEnd) {  // NOLINT
  RunEndToEndTests(false);
  RunEndToEndTests(true);
}

bool DoFilesMatch(
    const fs::path &first_file_name,
    const fs::path &second_file_name) {
  if (std::filesystem::file_size(first_file_name) !=
      std::filesystem::file_size(second_file_name))
  {
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

void SyncFile(
    const fs::path &source_file_name,
    bool compression_disabled,
    const fs::path &metadata_file_name,
    const fs::path &seed_data_file_name,
    const fs::path &temp_path_name,
    const fs::path &expected_output_file_name,
    // NOLINTNEXTLINE(misc-unused-parameters)
    const std::map<std::string, uint64_t> &expected_metrics,
    int threads) {
  std::string file_uri_prefix = "file://";
  fs::path output_file_name = temp_path_name / "syncd_output_file";
  auto sync_command = SyncCommand::Create(
      file_uri_prefix + source_file_name.string(),
      file_uri_prefix + metadata_file_name.string(),
      file_uri_prefix + seed_data_file_name.string(),
      output_file_name,
      compression_disabled,
      4,
      threads);
  auto return_code = sync_command->Run();
  CHECK(return_code == 0) << "Sync command failed for " +
                                 source_file_name.string();
  EXPECT_TRUE(DoFilesMatch(expected_output_file_name, output_file_name))
      << "Sync'd output file does not match expectations";
  // ExpectationCheckMetricVisitor(sync_command, std::move(expected_metrics));
}

void EndToEndFilesTest(
    const std::string &source_file_name,
    const std::string &seed_data_file_name,
    std::streamsize block_size,
    const std::string &expected_metadata_file_name,
    const std::string &expected_compressed_file_name,
    const std::string &expected_output_file_name,
    uint64_t expected_compressed_size,
    int threads) {
  LOG(INFO) << "E2E Files Test for " << source_file_name;
  LOG(INFO) << "Using seed file " << seed_data_file_name;

  auto tmp = ky::TempPath();
  auto temp_path_name = tmp.GetPath();
  auto metadata_file_name = temp_path_name / "i.ksync";
  auto compressed_file_name = temp_path_name / "i.pzst";
  auto return_code = PrepareCommand::Create(
                         source_file_name,
                         metadata_file_name,
                         compressed_file_name,
                         block_size,
                         threads)
                         ->Run();
  CHECK(return_code == 0) << "Prepare command failed for " + source_file_name;
  EXPECT_TRUE(DoFilesMatch(expected_metadata_file_name, metadata_file_name))
      << "Generated metadata files does not match expectations";
  EXPECT_TRUE(DoFilesMatch(expected_compressed_file_name, compressed_file_name))
      << "Generated compressed file does not match expectations";
  EXPECT_FALSE(DoFilesMatch(expected_metadata_file_name, compressed_file_name))
      << "Unexpected match of metadata and compressed files";

  std::map<std::string, uint64_t> expected_metrics = {
      {"//progress_current_bytes_", fs::file_size(source_file_name)},
      {"//progress_total_bytes_", fs::file_size(source_file_name)},
      {"//progress_compressed_bytes_", expected_compressed_size},
  };

  SyncFile(
      compressed_file_name,
      false,
      metadata_file_name,
      source_file_name,
      temp_path_name,
      expected_output_file_name,
      expected_metrics,
      threads);
}

void RunEndToEndFilesTestFor(
    const std::string &file_name,
    uint64_t expected_compressed_size,
    int threads) {
  static constexpr std::streamsize kBlockSize = 1024;

  EndToEndFilesTest(
      file_name,
      file_name,
      kBlockSize,
      file_name + ".ksync",
      file_name + ".pzst",
      file_name,
      expected_compressed_size,
      threads);
}

// Test summary:
// For a small file (less than block size) and a general file (a few blocks
//   plus a few additional bytes):
// 1. Run prepare command.
// 2. Confirm that the ksync and pzst files are as expected.
// 3. Use the generated ksync and pzst files to sync to a new output file.
// Ensure that the new output file matches the original file. A seed file is
// required and for this, the original file is used.
TEST_F(Tests, EndToEndFiles) {  // NOLINT
  std::string test_data_path = GetTestDataPath();
  LOG(INFO) << "Using test data path: " << test_data_path;

  static constexpr std::streamsize kExpectedCompressedSize = 42;

  RunEndToEndFilesTestFor(
      test_data_path + "/test_file_small.txt",
      kExpectedCompressedSize,
      kThreads);
  RunEndToEndFilesTestFor(
      test_data_path + "/test_file.txt",
      kExpectedCompressedSize,
      kThreads);
}

// Test summary:
// 1. Use a regular file as seed. Provide a ksync and a pzst file for a
//     new version (v2 file that has modifications on the original).
// 2. Run sync.
// Ensure that newly sync'd file matches the original non-compressed v2 file.
TEST_F(Tests, SyncFileFromSeed) {  // NOLINT
  std::string test_data_path = GetTestDataPath();
  LOG(INFO) << "Using test data path: " << test_data_path;

  std::string sync_file_name = test_data_path + "/test_file_v2.txt";
  std::string seed_file_name = test_data_path + "/test_file.txt";
  auto tmp = ky::TempPath();

  std::map<std::string, uint64_t> expected_metrics = {
      {"//progress_current_bytes_", 10340},
      {"//progress_total_bytes_", 10340},
      {"//progress_compressed_bytes_", 140},
  };

  SyncFile(
      sync_file_name + ".pzst",
      false,
      sync_file_name + ".ksync",
      seed_file_name,
      tmp.GetPath(),
      sync_file_name,
      expected_metrics,
      kThreads);
}

// Test syncing from a non-compressed file
TEST_F(Tests, SyncNonCompressedFile) {  // NOLINT
  std::string test_data_path = GetTestDataPath();
  LOG(INFO) << "Using test data path: " << test_data_path;

  std::string sync_file_name = test_data_path + "/test_file_v2.txt";
  std::string seed_file_name = test_data_path + "/test_file.txt";
  auto tmp = ky::TempPath();

  std::map<std::string, uint64_t> expected_metrics = {
      {"//progress_current_bytes_", 10340},
      {"//progress_total_bytes_", 10340},
      {"//progress_compressed_bytes_", 0},
  };

  SyncFile(
      sync_file_name,
      true,
      sync_file_name + ".ksync",
      seed_file_name,
      tmp.GetPath(),
      sync_file_name,
      expected_metrics,
      kThreads);
}

// Basic test to ensure different temp paths are returned.
// This does not do testing for race conditions.
TEST(SyncCommand, GetTempPath) {  // NOLINT
  auto p_1 = ky::TempPath();
  auto p_2 = ky::TempPath();
  EXPECT_NE(p_1.GetPath(), p_2.GetPath());
}

//TEST(SyncCommand, ThreadPool) {  // NOLINT
//  int num_threads = 4;
//  boost::asio::thread_pool pool(num_threads);
//  std::atomic<int> value(0);
//  for (int i = 0; i < num_threads * 2; i++) {
//    boost::asio::post(pool, [&value]() {
//      LOG(INFO) << "Incrementing value";
//      value += 1;
//    });
//  }
//  pool.join();
//  EXPECT_EQ(value, num_threads * 2);
//}

}  // namespace kysync