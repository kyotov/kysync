#include <gtest/gtest.h>

#include "../utilities/safe_cast.h"

namespace kysync {

TEST(SafeCastTests, int) {  // NOLINT
  int x = std::numeric_limits<int>::max();
  unsigned int y = x;
  y++;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  EXPECT_DEATH(SafeCast<int>(y), "Assert failed: value <= max");

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto)
  EXPECT_EQ(x, SafeCast<int>(y - 1));
}

TEST(SafeCastTests, int32) {  // NOLINT
  int32_t x = std::numeric_limits<int32_t>::max();
  uint32_t y = x;
  y++;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  EXPECT_DEATH(SafeCast<int32_t>(y), "Assert failed: value <= max");

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto)
  EXPECT_EQ(x, SafeCast<int32_t>(y - 1));
}

TEST(SafeCastTests, int64) {  // NOLINT
  int64_t x = std::numeric_limits<int64_t>::max();
  uint64_t y = x;
  y++;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  EXPECT_DEATH(SafeCast<int64_t>(y), "Assert failed: value <= max");

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto)
  EXPECT_EQ(x, SafeCast<int64_t>(y - 1));
}

TEST(SafeCastTests, mixed) {  // NOLINT
  int32_t x = std::numeric_limits<int32_t>::max();
  uint64_t y = x;
  y++;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  EXPECT_DEATH(SafeCast<int32_t>(y), "Assert failed: value <= max");

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto)
  EXPECT_EQ(x, SafeCast<int32_t>(y - 1));
}

}  // namespace kysync