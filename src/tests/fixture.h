#ifndef KSYNC_SRC_TESTS_FIXTURE_H
#define KSYNC_SRC_TESTS_FIXTURE_H

#include <gtest/gtest.h>
#include <ky/temp_path.h>

#include <filesystem>

namespace kysync {

class Fixture : public ::testing::Test {
public:
  Fixture();

  [[nodiscard]] std::filesystem::path GetScratchPath() const;

private:
  ky::TempPath temp_path_;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_FIXTURE_H
