#include <gtest/gtest.h>
#include <kysync/test_commands/system_command.h>
#include <kysync/test_common/test_environment.h>

#include <random>

#include "performance_test_fixture.h"

namespace kysync {

TestEnvironment* test_environment = dynamic_cast<TestEnvironment*>(  // NOLINT
    testing::AddGlobalTestEnvironment(new TestEnvironment()));       // NOLINT

#define LinuxCacheTests DISABLED_LinuxCacheTests  // NOLINT

class LinuxCacheTests : public PerformanceTestFixture {
  static void CreateFile(
      const std::filesystem::path& path,
      int block_count,
      int block_size) {
    auto output = std::ofstream(path);
    auto buffer = std::vector<char>(block_size);
    auto random = std::default_random_engine(block_size);
    for (auto i = 0; i < block_size; ++i) {
      buffer[i] = static_cast<char>(random());
    }
    for (auto i = 0; i < block_count; ++i) {
      output.write(buffer.data(), block_size);
    }
  }

protected:
  static const int k_ = 1024;

  static void SetUpTestSuite() {
    PerformanceTestFixture::SetUpTestSuite();

    temp_path_ =
        std::make_unique<ky::TempPath>(TestEnvironment::GetTmpRoot(), false);

    auto g = TestEnvironment::GetEnvInt("TEST_FILE_SIZE_GB", 10);
    CreateFile(temp_path_->GetPath() / "data.bin", g * k_, k_ * k_);
  }

  static void TearDownTestSuite() {
    temp_path_.reset();

    PerformanceTestFixture::TearDownTestSuite();
  }

private:
  static std::unique_ptr<ky::TempPath> temp_path_;  // NOLINT
};

std::unique_ptr<ky::TempPath> LinuxCacheTests::temp_path_;  // NOLINT

TEST_F(LinuxCacheTests, Cached) {  // NOLINT
  Run(std::move(SystemCommand("cached_cat", "cat data.bin > /dev/null")));
}

TEST_F(LinuxCacheTests, NotCached) {  // NOLINT
  FlushCaches();
  Run(std::move(SystemCommand("not_cached_cat", "cat data.bin > /dev/null")));
}

}  // namespace kysync
