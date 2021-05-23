#include <glog/logging.h>
#include <gtest/gtest.h>
#include <httplib.h>
#include <kysync/path_config.h>
#include <zstd.h>

#include <filesystem>
#include <fstream>

#include "../checksums/strong_checksum.h"
#include "../checksums/weak_checksum.h"
#include "../commands/impl/prepare_command_impl.h"
#include "../commands/impl/sync_command_impl.h"
#include "../commands/prepare_command.h"
#include "../commands/sync_command.h"
#include "../config.h"
#include "../readers/file_reader.h"
#include "../readers/http_reader.h"
#include "../readers/memory_reader.h"
#include "expectation_check_metrics_visitor.h"
#include "utilities/fixture.h"
#include "utilities/temp_path.h"

namespace kysync {

namespace fs = std::filesystem;

class Tests : public Fixture {};

TEST_F(Tests, SimpleWeakChecksum) {  // NOLINT
  const auto *data = "0123456789";
  auto wcs = WeakChecksum(data, strlen(data));
  EXPECT_EQ(wcs, 183829005);
}

TEST_F(Tests, RollingWeakChecksum) {  // NOLINT
  char data[] = "012345678901234567890123456789";
  ASSERT_EQ(strlen(data), 30);

  auto size = strlen(data) / 3;
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
  auto scs = StrongChecksum::Compute(data, strlen(data));
  EXPECT_EQ(scs.ToString(), "e353667619ec664b49655fc9692165fb");
}

TEST_F(Tests, StreamingStringChecksum) {  // NOLINT
  constexpr int kCount = 10'000;
  std::stringstream s;

  const auto *data = "0123456789";

  for (int i = 0; i < kCount; i++) {
    s.write(data, strlen(data));
  }

  auto str = s.str();
  ASSERT_EQ(str.length(), kCount * strlen(data));

  const auto *buffer = str.c_str();

  auto scs_1 = StrongChecksum::Compute(buffer, kCount * strlen(data));

  s.seekg(0);
  auto scs_2 = StrongChecksum::Compute(s);

  EXPECT_EQ(scs_1, scs_2);
}

void TestReader(Reader &reader, size_t expected_size) {
  ASSERT_EQ(reader.GetSize(), expected_size);

  char buffer[1024];

  {
    auto count = reader.Read(buffer, 1, 3);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer, "123");
  }

  {
    auto count = reader.Read(buffer, 8, 3);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer, "89");
  }

  {
    auto count = reader.Read(buffer, 20, 5);
    buffer[count] = '\0';
    EXPECT_STREQ(buffer, "");
  }

  ExpectationCheckMetricVisitor(
      reader,
      {//
       {"//total_reads_", 3},
       {"//total_bytes_read_", 5}});
}

std::string CreateMemoryReaderUri(const void *address, size_t size) {
  std::stringstream result;
  result << "memory://" << address << ":" << std::hex << size;
  return result.str();
}

std::string CreateMemoryReaderUri(const std::string &data) {
  return CreateMemoryReaderUri(data.data(), data.size());
}

TEST_F(Tests, MemoryReaderSimple) {  // NOLINT
  const auto *data = "0123456789";

  auto reader = MemoryReader(data, strlen(data));
  TestReader(reader, strlen(data));

  auto uri = CreateMemoryReaderUri(data, strlen(data));
  TestReader(*Reader::Create(uri), strlen(data));
}

TEST_F(Tests, FileReaderSimple) {  // NOLINT
  const auto *data = "0123456789";

  TempPath tmp;
  auto path = tmp.GetPath() / "data.bin";

  std::ofstream f(path, std::ios::binary);
  f.write(data, strlen(data));
  f.close();

  auto reader = FileReader(path);
  TestReader(reader, fs::file_size(path));
  TestReader(*Reader::Create("file://" + path.string()), strlen(data));
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
  f.write(content.data(), content.size());
}

std::string ReadFile(const fs::path &path) {
  auto result = std::string();
  result.resize(fs::file_size(path));

  auto f = std::ifstream(path, std::ios::binary);
  f.read(result.data(), result.size());

  return result;
}

class KySyncTest {
public:
  static const std::vector<uint32_t> &ExamineWeakChecksums(
      const PrepareCommand &c) {
    return c.impl_->weak_checksums_;
  }

  static const std::vector<StrongChecksum> &ExamineStrongChecksums(
      const PrepareCommand &c) {
    return c.impl_->strong_checksums_;
  }

  static const std::vector<uint32_t> &ExamineWeakChecksums(
      const SyncCommand &c) {
    return c.impl_->weak_checksums_;
  }

  static const std::vector<StrongChecksum> &ExamineStrongChecksums(
      const SyncCommand &c) {
    return c.impl_->strong_checksums_;
  }

  static void ReadMetadata(const SyncCommand &c) { c.impl_->ReadMetadata(); }

  static std::vector<size_t> ExamineAnalisys(const SyncCommand &c) {
    std::vector<size_t> result;

    for (size_t i = 0; i < c.impl_->block_count_; i++) {
      auto wcs = c.impl_->weak_checksums_[i];
      auto data = c.impl_->analysis_[wcs];
      result.push_back(data.seed_offset);
    }

    return result;
  }

  static int GetCompressionLevel(const PrepareCommand &c) {
    return c.impl_->kCompressionLevel;
  }

  static size_t GetBlockSize(const PrepareCommand &c) {
    return c.impl_->kBlockSize;
  }
};

std::stringstream CreateInputStream(const std::string &data) {
  std::stringstream result;
  result.write(data.data(), data.size());
  result.seekg(0);
  return result;
}

size_t GetExpectedCompressedSize(
    const std::string &data,
    int compression_level,
    size_t block_size) {
  auto compression_buffer_size = ZSTD_compressBound(block_size);
  auto unique_compression_buffer =
      std::make_unique<char[]>(compression_buffer_size);
  size_t compressed_size = 0;
  size_t size_read = 0;
  while (size_read < data.size()) {
    auto size_remaining = data.size() - size_read;
    size_t size_to_read = std::min(block_size, size_remaining);
    compressed_size += ZSTD_compress(
        unique_compression_buffer.get(),
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

  auto tmp = TempPath();
  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";

  WriteFile(data_path, data);
  auto c = PrepareCommand(data_path, kysync_path, pzst_path, block);
  c.Run();

  const auto &wcs = KySyncTest::ExamineWeakChecksums(c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(WeakChecksum("0123", block), wcs[0]);
  EXPECT_EQ(WeakChecksum("4567", block), wcs[1]);
  EXPECT_EQ(WeakChecksum("89\0\0", block), wcs[2]);

  const auto &scs = KySyncTest::ExamineStrongChecksums(c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(StrongChecksum::Compute("0123", block), scs[0]);
  EXPECT_EQ(StrongChecksum::Compute("4567", block), scs[1]);
  EXPECT_EQ(StrongChecksum::Compute("89\0\0", block), scs[2]);
}

TEST_F(Tests, SimplePrepareCommand2) {  // NOLINT
  auto data = std::string("123412341234");
  auto block = 4;
  auto block_count = (data.size() + block - 1) / block;

  auto tmp = TempPath();
  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";

  WriteFile(data_path, data);
  auto c = PrepareCommand(data_path, kysync_path, pzst_path, block);
  c.Run();

  const auto &wcs = KySyncTest::ExamineWeakChecksums(c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(WeakChecksum("1234", block), wcs[0]);
  EXPECT_EQ(WeakChecksum("1234", block), wcs[1]);
  EXPECT_EQ(WeakChecksum("1234", block), wcs[2]);

  const auto &scs = KySyncTest::ExamineStrongChecksums(c);
  EXPECT_EQ(wcs.size(), block_count);
  EXPECT_EQ(StrongChecksum::Compute("1234", block), scs[0]);
  EXPECT_EQ(StrongChecksum::Compute("1234", block), scs[1]);
  EXPECT_EQ(StrongChecksum::Compute("1234", block), scs[2]);
}

TEST_F(Tests, MetadataRoundtrip) {  // NOLINT
  auto block = 4;
  std::string data = "0123456789";

  auto tmp = TempPath();
  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";
  auto output_path = tmp.GetPath() / "output.bin";

  WriteFile(data_path, data);
  auto pc = PrepareCommand(data_path, kysync_path, pzst_path, block);
  pc.Run();

  auto sc = SyncCommand(
      "file://" + data_path.string(),
      "file://" + kysync_path.string(),
      "file://" + data_path.string(),
      output_path,
      true,
      4,
      1);

  KySyncTest::ReadMetadata(sc);

  // expect that the checksums had a successful round trip
  EXPECT_EQ(
      KySyncTest::ExamineWeakChecksums(pc),
      KySyncTest::ExamineWeakChecksums(sc));
  EXPECT_EQ(
      KySyncTest::ExamineStrongChecksums(pc),
      KySyncTest::ExamineStrongChecksums(sc));
}

void EndToEndTest(
    const std::string &data,
    const std::string &seed_data,
    bool compression_disabled,
    size_t block,
    const std::vector<size_t> &expected_block_mapping) {
  LOG(INFO) << "E2E for " << data.substr(0, 40);

  auto tmp = TempPath();

  auto data_path = tmp.GetPath() / "data.bin";
  auto kysync_path = tmp.GetPath() / "data.bin.kysync";
  auto pzst_path = tmp.GetPath() / "data.bin.pzst";
  auto seed_data_path = tmp.GetPath() / "seed_data.bin";
  auto output_path = tmp.GetPath() / "output.bin";

  WriteFile(data_path, data);
  WriteFile(seed_data_path, seed_data);
  PrepareCommand(data_path, kysync_path, pzst_path, block).Run();

  SyncCommand(
      "file://" + (compression_disabled ? data_path : pzst_path).string(),
      "file://" + kysync_path.string(),
      "file://" + data_path.string(),
      output_path,
      compression_disabled,
      4,
      1)
      .Run();

  EXPECT_EQ(data, ReadFile(output_path));
}

void RunEndToEndTests(bool compression_disabled) {
  // FIXME: research if we can do parametrized testing...
  std::string data = "0123456789";
  EndToEndTest(data, data, compression_disabled, 4, {0, 4, -1ULL /*8*/});
  EndToEndTest(data, data, compression_disabled, 6, {0, -1ULL /*6*/});

  EndToEndTest(
      "0123456789",
      "001234004567",
      compression_disabled,
      4,
      {1, 8, -1ULL});
  EndToEndTest("123412341234", "00123400", compression_disabled, 4, {2, 2, 2});
  EndToEndTest("12345678", "", compression_disabled, 4, {-1ULL, -1ULL});
  EndToEndTest(
      "abcdefjhijklmnopqrstuvwxyz",
      "_qrst_mnop_ijkl_abcd_efjh_uvwx_yz",
      compression_disabled,
      4,
      {16, 21, 11, 6, 1, 26, -1ULL /*31*/});

  if (kWarmupAfterMatch) {  // NOLINT(readability-simplify-boolean-expr)
    EndToEndTest(
        "1234234534564567567867897890",
        "1234567890",
        compression_disabled,
        4,
        {0, -1ULL, -1ULL, -1ULL, 4, -1ULL, -1ULL});
  } else {
    EndToEndTest(
        "1234234534564567567867897890",
        "1234567890",
        compression_disabled,
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
    const bool compression_disabled,
    const fs::path &metadata_file_name,
    const fs::path &seed_data_file_name,
    const fs::path &temp_path_name,
    const fs::path &expected_output_file_name,
    std::map<std::string, uint64_t> &&expected_metrics) {
  std::string file_uri_prefix = "file://";
  fs::path output_file_name = temp_path_name / "syncd_output_file";
  auto sync_command = SyncCommand(
      file_uri_prefix + source_file_name.string(),
      file_uri_prefix + metadata_file_name.string(),
      file_uri_prefix + seed_data_file_name.string(),
      output_file_name,
      compression_disabled,
      4,
      32);
  auto return_code = sync_command.Run();
  CHECK(return_code == 0) << "Sync command failed for " +
                                 source_file_name.string();
  EXPECT_TRUE(DoFilesMatch(expected_output_file_name, output_file_name))
      << "Sync'd output file does not match expectations";
  // ExpectationCheckMetricVisitor(sync_command, std::move(expected_metrics));
}

void EndToEndFilesTest(
    const std::string &source_file_name,
    const std::string &seed_data_file_name,
    size_t block_size,
    const std::string &expected_metadata_file_name,
    const std::string &expected_compressed_file_name,
    const std::string &expected_output_file_name,
    uint64_t expected_compressed_size) {
  LOG(INFO) << "E2E Files Test for " << source_file_name;
  LOG(INFO) << "Using seed file " << seed_data_file_name;

  TempPath tmp;
  auto temp_path_name = tmp.GetPath();
  auto metadata_file_name = temp_path_name / "i.ksync";
  auto compressed_file_name = temp_path_name / "i.pzst";
  auto return_code = PrepareCommand(
                         source_file_name,
                         metadata_file_name,
                         compressed_file_name,
                         block_size)
                         .Run();
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
      std::move(expected_metrics));
}

void RunEndToEndFilesTestFor(
    const std::string &file_name,
    uint64_t expected_compressed_size) {
  EndToEndFilesTest(
      file_name,
      file_name,
      1024,
      file_name + ".ksync",
      file_name + ".pzst",
      file_name,
      expected_compressed_size);
}

std::string GetTestDataPath() {
  // NOTE: This expects that either the test data path is provided through
  // the TEST_DATA_DIR environment variable or that the test data dir exists one
  // level above the test's working directory
  auto *dir_name = std::getenv("TEST_DATA_DIR");
  return dir_name == nullptr ? (CMAKE_SOURCE_DIR / "test_data").string()
                             : dir_name;
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

  const uint64_t kExpectedCompressedSize = 42;
  RunEndToEndFilesTestFor(
      test_data_path + "/test_file_small.txt",
      kExpectedCompressedSize);
  RunEndToEndFilesTestFor(
      test_data_path + "/test_file.txt",
      kExpectedCompressedSize);
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
  TempPath tmp;

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
      std::move(expected_metrics));
}

// Test syncing from a non-compressed file
TEST_F(Tests, SyncNonCompressedFile) {  // NOLINT
  std::string test_data_path = GetTestDataPath();
  LOG(INFO) << "Using test data path: " << test_data_path;

  std::string sync_file_name = test_data_path + "/test_file_v2.txt";
  std::string seed_file_name = test_data_path + "/test_file.txt";
  TempPath tmp;

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
      std::move(expected_metrics));
}

// Basic test to ensure different temp paths are returned.
// This does not do testing for race conditions.
TEST(SyncCommand, GetTempPath) {  // NOLINT
  TempPath sample_paths[2];
  EXPECT_NE(sample_paths[0].GetPath(), sample_paths[1].GetPath());
}

// Note: This function and the following 2 tests are temporary and will be
// removed after http_tests.cc have been pushed
void HttpClientMultirangeTest(
    httplib::Ranges ranges,
    size_t expected_body_size) {
  auto range_header = httplib::make_range_header(ranges);
  httplib::Client http_client("http://mirror.math.princeton.edu");
  std::string path("/pub/ubuntu-iso/20.04/ubuntu-20.04.2.0-desktop-amd64.list");
  auto res = http_client.Get(path.c_str(), {range_header});
  CHECK(res.error() == httplib::Error::Success);
  CHECK_EQ(res->status, 206);
  LOG(INFO) << "Got body: " << res->body;
  EXPECT_EQ(res->body.size(), expected_body_size);
}

TEST(HttplibRetrieval, MultirangeContiguous) {
  auto block_size = 4;
  httplib::Ranges ranges;
  ranges.push_back({0, block_size - 1});
  ranges.push_back({block_size, block_size * 2 - 1});
  std::string expected_string("/isolinu");
  HttpClientMultirangeTest(ranges, expected_string.size());
}

TEST(HttplibRetrieval, MultirangeNoncontiguous) {
  auto block_size = 4;
  httplib::Ranges ranges;
  ranges.push_back({0, block_size - 1});
  ranges.push_back({block_size + 1, block_size * 2});
  std::string expected_string("/isoinux");
  HttpClientMultirangeTest(ranges, 222);
}

// Note: This function and the following 2 tests are temporary and will be
// removed after http_tests.cc have been pushed
void HttpReaderMultirangeTest(
    std::vector<BatchRetrivalInfo> &batched_retrieval_infos,
    std::string &expected_string) {
  std::string uri(
      "http://mirror.math.princeton.edu/pub/ubuntu-iso/20.04/"
      "ubuntu-20.04.2.0-desktop-amd64.list");
  auto reader = Reader::Create(uri);
  size_t total_size = 0;
  for (auto &retrieval_info : batched_retrieval_infos) {
    total_size += retrieval_info.size_to_read;
  }
  auto buffer = std::make_unique<char[]>(total_size);
  auto size_read = reader.get()->Read(buffer.get(), batched_retrieval_infos);
  EXPECT_EQ(size_read, expected_string.size());
  EXPECT_TRUE(
      std::memcmp(
          buffer.get(),
          expected_string.c_str(),
          expected_string.size()) == 0);
}

TEST(HttpReaderRetrieval, MultirangeContiguous) {
  std::vector<BatchRetrivalInfo> batched_retrieval_infos;
  size_t block_size = 4;
  batched_retrieval_infos.push_back(
      {.block_index = 0,
       .source_begin_offset = 0,
       .size_to_read = block_size,
       .offset_to_write_to = 0});
  batched_retrieval_infos.push_back(
      {.block_index = 1,
       .source_begin_offset = block_size,
       .size_to_read = block_size,
       .offset_to_write_to = block_size});
  std::string expected_string("/isolinu");
  HttpReaderMultirangeTest(batched_retrieval_infos, expected_string);
}

TEST(HttpReaderRetrieval, MultirangeNoncontiguous) {
  std::vector<BatchRetrivalInfo> batched_retrieval_infos;
  size_t block_size = 4;
  batched_retrieval_infos.push_back(
      {.block_index = 0,
       .source_begin_offset = 0,
       .size_to_read = block_size,
       .offset_to_write_to = 0});
  batched_retrieval_infos.push_back(
      {.block_index = 1,
       .source_begin_offset = block_size + 1,
       .size_to_read = block_size,
       .offset_to_write_to = block_size});
  batched_retrieval_infos.push_back(
      {.block_index = 1,
       .source_begin_offset = block_size * 2 + 2,
       .size_to_read = block_size,
       .offset_to_write_to = block_size * 2});
  std::string expected_string("/isoinuxadtx");
  HttpReaderMultirangeTest(batched_retrieval_infos, expected_string);
}

}  // namespace kysync