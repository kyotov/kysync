#ifndef KSYNC_SRC_TESTS_TEST_ENVIRONMENT_H
#define KSYNC_SRC_TESTS_TEST_ENVIRONMENT_H

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace kysync {

class TestEnvironment : public ::testing::Environment {
public:
  ~TestEnvironment() override;

  void SetUp() override;

  void TearDown() override;

  std::ofstream &GetPerfLog();

  [[nodiscard]] static std::filesystem::path GetTmpRoot();

  [[nodiscard]] static std::string GetEnv(
      const std::string &name,
      const std::string &default_value);

  [[nodiscard]] static std::filesystem::path GetEnv(
      const std::string &name,
      const std::filesystem::path &default_value);

  [[nodiscard]] static intmax_t GetEnv(
      const std::string &name,
      intmax_t default_value);

  [[nodiscard]] static int GetEnvInt(
      const std::string &name,
      int default_value);

private:
  std::ofstream perf_log_;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_TEST_ENVIRONMENT_H
