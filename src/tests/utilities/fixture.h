#ifndef KSYNC_FIXTURE_H
#define KSYNC_FIXTURE_H

#include <gtest/gtest.h>

namespace kysync {

class Fixture : public ::testing::Test {
protected:
  static void SetUpTestSuite();

  static std::string GetEnv(
      const std::string &name,
      const std::string &default_value);

  static uintmax_t GetEnv(const std::string &name, uintmax_t default_value);
};

}  // namespace kysync

#endif  // KSYNC_FIXTURE_H
