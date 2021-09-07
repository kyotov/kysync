#ifndef KSYNC_SRC_TESTS_PERFORMANCE_TEST_FIXTURE_H
#define KSYNC_SRC_TESTS_PERFORMANCE_TEST_FIXTURE_H

#include <ky/temp_path.h>
#include <kysync/commands/command.h>
#include <kysync/test_common/test_fixture.h>

#include <filesystem>
#include <fstream>

namespace kysync {

class PerformanceTestFixture : public Fixture {
protected:
  static void Run(Command &&command);

public:
  static void FlushCaches();
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_PERFORMANCE_TEST_FIXTURE_H
