#include "performance_test_fixture.h"

#include <glog/logging.h>
#include <ky/observability/observer.h>
#include <kysync/test_common/test_environment.h>

namespace kysync {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern TestEnvironment *test_environment;

void PerformanceTestFixture::FlushCaches() {
  LOG(INFO) << "cache flush result: "
            << system(  // NOLINT(cert-env33-c, concurrency-mt-unsafe)
                   "sudo sh -c \"/usr/bin/echo 3 > /proc/sys/vm/drop_caches\"");
}

void PerformanceTestFixture::Run(Command &&command) {
  auto monitor = ky::observability::Observer(command);

  EXPECT_EQ(monitor.Run([&command]() { return command.Run(); }), 0);

  auto &perf_log = test_environment->GetPerfLog();

  perf_log << std::endl;
  perf_log << "name=" << command.GetName() << std::endl;

  monitor.SnapshotPhases([&perf_log](auto key, auto value) {
    perf_log << key << "=" << value << std::endl;
  });
}

}  // namespace kysync
