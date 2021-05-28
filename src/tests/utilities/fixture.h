#ifndef KSYNC_FIXTURE_H
#define KSYNC_FIXTURE_H

#include <gtest/gtest.h>

#include <filesystem>

namespace kysync {

class Fixture : public ::testing::Test {
protected:
  static void SetUpTestSuite();

public:
  static std::string GetEnv(
      const std::string &name,
      const std::string &default_value);

  static std::filesystem::path GetEnv(
      const std::string &name,
      const std::filesystem::path &default_value);

  static intmax_t GetEnv(const std::string &name, intmax_t default_value);

  static int GetEnvInt(const std::string &name, int default_value);
};

}  // namespace kysync

#endif  // KSYNC_FIXTURE_H
