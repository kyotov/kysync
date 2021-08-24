#ifndef KSYNC_SRC_TESTS_PERFORMANCE_TEST_FIXTURE_H
#define KSYNC_SRC_TESTS_PERFORMANCE_TEST_FIXTURE_H

#include <ky/temp_path.h>

#include <filesystem>
#include <fstream>

#include "fixture.h"
#include "kysync/commands/command.h"

namespace kysync {

class PerformanceTestFixture : public Fixture {
protected:
  void Run(Command &&command);

public:
  static void FlushCaches();
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_PERFORMANCE_TEST_FIXTURE_H
