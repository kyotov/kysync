#include <gtest/gtest.h>

#include <atomic>

#include "wcs.h"

TEST(WeakChecksum, Simple)
{
  auto data = "0123456789";
  auto wcs = weakChecksum(data, strlen(data));
  EXPECT_EQ(wcs, 183829005);
}

TEST(WeakChecksum, Rolling)
{
  char data[] = "012345678901234567890123456789";
  EXPECT_EQ(strlen(data), 30);

  auto size = strlen(data) / 3;
  memset(data, 0, size);

  std::atomic<int> count = 0;

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
