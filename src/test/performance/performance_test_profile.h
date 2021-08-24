#ifndef KSYNC_SRC_TESTS_PERFORMANCE_TEST_PROFILE_H
#define KSYNC_SRC_TESTS_PERFORMANCE_TEST_PROFILE_H

#include <string>

namespace kysync {

struct PerformanceTestProfile final {
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::string tag;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::streamsize data_size;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::streamsize seed_data_size;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::streamsize fragment_size;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::streamsize block_size;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  int blocks_in_batch;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  int similarity;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  int threads;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  bool compression;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  bool http;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  bool zsync;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  bool flush_caches;

  PerformanceTestProfile();

  PerformanceTestProfile(
      std::string tag,
      std::streamsize data_size,
      std::streamsize seed_data_size,
      std::streamsize fragment_size,
      std::streamsize block_size,
      int blocks_in_batch,
      int similarity,
      int threads,
      bool compression,
      bool http,
      bool zsync,
      bool flush_caches);
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_PERFORMANCE_TEST_PROFILE_H
