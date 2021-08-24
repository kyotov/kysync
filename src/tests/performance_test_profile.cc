#include "performance_test_profile.h"

#include "test_environment.h"

namespace kysync {

PerformanceTestProfile::PerformanceTestProfile(
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
    bool flush_caches)
    : tag(std::move(tag)),
      data_size(data_size),
      seed_data_size(seed_data_size),
      fragment_size(fragment_size),
      block_size(block_size),
      blocks_in_batch(blocks_in_batch),
      similarity(similarity),
      threads(threads),
      compression(compression),
      http(http),
      zsync(zsync),
      flush_caches(flush_caches) {}

PerformanceTestProfile::PerformanceTestProfile()
    : PerformanceTestProfile(
          TestEnvironment::GetEnv("TEST_TAG", std::string("default")),
          TestEnvironment::GetEnv("TEST_DATA_SIZE", 1'000'000),
          TestEnvironment::GetEnv("TEST_SEED_DATA_SIZE", -1),
          TestEnvironment::GetEnv("TEST_FRAGMENT_SIZE", 123'456),
          TestEnvironment::GetEnv("TEST_BLOCK_SIZE", 16'384),
          TestEnvironment::GetEnvInt("TEST_BLOCKS_IN_BATCH", 4),
          TestEnvironment::GetEnvInt("TEST_SIMILARITY", 90),
          TestEnvironment::GetEnvInt("TEST_THREADS", 32),
          TestEnvironment::GetEnv("TEST_COMPRESSION", false),
          TestEnvironment::GetEnv("TEST_HTTP", false),
          TestEnvironment::GetEnv("TEST_ZSYNC", false),
          TestEnvironment::GetEnv("TEST_FLUSH_CACHES", true)) {}

}  // namespace kysync