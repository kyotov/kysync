#include <glog/logging.h>
#include <gtest/gtest.h>
#include <kysync/path_config.h>

#include <chrono>
#include <fstream>

#include "../commands/prepare_command.h"
#include "../commands/sync_command.h"
#include "../monitor.h"
#include "../utilities/timer.h"
#include "commands/gen_data_command.h"
#include "commands/system_command.h"
#include "http_server/http_server.h"
#include "utilities/fixture.h"
#include "utilities/temp_path.h"

// with inspiration from
// https://www.sandordargo.com/blog/2019/04/24/parameterized-testing-with-gtest

namespace kysync {

class Performance : public Fixture {
protected:
  fs::path path;
  size_t data_size;
  size_t seed_data_size;
  size_t fragment_size;
  size_t block_size;
  int similarity;
  int num_blocks_in_batch;
  int threads;
  bool compression;
  bool identity_reconstruction;

  Performance() {
    auto root_path = GetEnv("TEST_ROOT_DIR", CMAKE_BINARY_DIR);
    data_size = GetEnv("TEST_DATA_SIZE", 1'000'000ULL);
    seed_data_size = GetEnv("TEST_SEED_DATA_SIZE", -1ULL);
    fragment_size = GetEnv("TEST_FRAGMENT_SIZE", 123'456);
    block_size = GetEnv("TEST_BLOCK_SIZE", 16'384);
    similarity = GetEnv("TEST_SIMILARITY", 90);
    num_blocks_in_batch = GetEnv("TEST_NUM_BLOCKS_IN_BATCH", 4);
    threads = GetEnv("TEST_THREADS", 32);
    compression = GetEnv("TEST_COMPRESSION", false);
    identity_reconstruction = GetEnv("TEST_IDENTITY_RECONSTRUCTION", false);
  }

  using GetUriCallback = std::function<std::string(const fs::path &)>;

  void Execute(
      const TempPath &tmp,
      MetricContainer *metric_container,
      const GetUriCallback &get_uri_callback);

  void Execute(bool use_http);

  void ExecuteZsync();
};

#define PERFLOG(X) #X << "=" << X << std::endl

void MetricSnapshot(
    std::ofstream &log,
    Command &command,
    MetricContainer *metric_container) {
  auto monitor = Monitor(command);

  if (metric_container != nullptr) {
    monitor.RegisterMetricContainer(
        typeid(*metric_container).name(),
        *metric_container);
  }

  EXPECT_EQ(monitor.Run(), 0);

  const auto *command_name = typeid(command).name();
  monitor.MetricSnapshot([&](std::string key, auto value) {
    // TODO: clean-up again... removed to see HttpServer data
    //    if (key.starts_with("//phases")) {
    log << command_name << ":" << key << "=" << value << std::endl;
    //    }
  });
};

using GetUriCallback = std::function<std::string(const fs::path &)>;

// FIXME: passing the metric_container is kind of ugly... find a better way
void Performance::Execute(
    const TempPath &tmp,
    MetricContainer *metric_container,
    const GetUriCallback &get_uri_callback) {
  const fs::path kRootPath = tmp.GetPath();
  const fs::path kDataPath = kRootPath / "data.bin";
  const fs::path kSeedDataPath =
      identity_reconstruction ? kDataPath : kRootPath / "seed_data.bin";
  const fs::path kKysyncPath = kRootPath / "data.bin.kysync";
  const fs::path kPzstPath = kRootPath / "data.bin.pzst";
  const fs::path kOutPath = kRootPath / "data.bin.out";
  const fs::path kLogPath = GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");

  std::ofstream log(kLogPath / "perf.log", std::ios::app);

  log << std::endl                //
      << PERFLOG(path)            //
      << PERFLOG(data_size)       //
      << PERFLOG(seed_data_size)  //
      << PERFLOG(fragment_size)   //
      << PERFLOG(block_size)      //
      << PERFLOG(similarity)      //
      << PERFLOG(threads)         //
      << PERFLOG(compression)     //
      << PERFLOG(identity_reconstruction);

  auto gen_data = GenDataCommand(  //
      tmp.GetPath(),
      data_size,
      seed_data_size,
      fragment_size,
      similarity);
  MetricSnapshot(log, gen_data, nullptr);

  auto prepare = PrepareCommand(  //
      kDataPath,
      kKysyncPath,
      kPzstPath,
      block_size);
  MetricSnapshot(log, prepare, nullptr);

  auto sync = SyncCommand(  //
      get_uri_callback(kDataPath),
      get_uri_callback(kKysyncPath),
      "file://" + kSeedDataPath.string(),  // seed is always local
      kOutPath,
      !compression,
      num_blocks_in_batch,
      threads);
  MetricSnapshot(log, sync, metric_container);
}

void Performance::Execute(bool use_http) {
  const auto tmp = TempPath(path, false);

  if (use_http) {
    auto server = HttpServer(tmp.GetPath(), 8000, true);
    Execute(tmp, &server, [](auto path) {
      return "http://localhost:8000/" + path.filename().string();
    });
  } else {
    Execute(tmp, nullptr, [](auto path) { return "file://" + path.string(); });
  }
}

void Performance::ExecuteZsync() {
  const auto tmp = TempPath(path, true);
  auto server = HttpServer(tmp.GetPath(), 8000, true);

  const fs::path kRootPath = tmp.GetPath();
  const fs::path kDataPath = kRootPath / "data.bin";
  const fs::path kSeedDataPath =
      identity_reconstruction ? kDataPath : kRootPath / "seed_data.bin";
  const fs::path kZsyncPath = kRootPath / "data.bin.kysync";
  const fs::path kPzstPath = kRootPath / "data.bin.pzst";
  const fs::path kOutPath = kRootPath / "data.bin.out";

  std::ofstream log("perf.log", std::ios::app);

  log << std::endl                //
      << PERFLOG(path)            //
      << PERFLOG(data_size)       //
      << PERFLOG(seed_data_size)  //
      << PERFLOG(fragment_size)   //
      << PERFLOG(block_size)      //
      << PERFLOG(similarity)      //
      << PERFLOG(threads)         //
      << PERFLOG(compression)     //
      << PERFLOG(identity_reconstruction);

  const fs::path zsync_path = GetEnv("ZSYNC_BIN_DIR", CMAKE_BINARY_DIR / "zsync/bin");

  if (!fs::exists(zsync_path / "zsync")) {
    LOG(WARNING) << "zsync binary not found, skipping zsync test (are you on Windows!?)";
    return;
  }

  auto gen_data = GenDataCommand(  //
      tmp.GetPath(),
      data_size,
      seed_data_size,
      fragment_size,
      similarity);
  MetricSnapshot(log, gen_data, &server);

  {
    std::stringstream command;
    command << zsync_path << "/zsyncmake " << kDataPath  //
            << " -o " << kRootPath / "data.bin.zsync"    //
            << " -u data.bin";
    LOG(INFO) << command.str();
    auto prepare = SystemCommand(command.str());
    MetricSnapshot(log, prepare, &server);
  }

  {
    std::stringstream command;
    command << zsync_path << "/zsync -q"             //
            << " -i " << kSeedDataPath               //
            << " -o " << kRootPath / "data.bin.out"  //
            << " http://localhost:8000/data.bin.zsync";
    LOG(INFO) << command.str();
    auto sync = SystemCommand(command.str());
    MetricSnapshot(log, sync, &server);
  }
}

TEST_F(Performance, Simple) {  // NOLINT
  Execute(false);
}

TEST_F(Performance, Http) {  // NOLINT
  Execute(true);
}

TEST_F(Performance, Zsync) {  // NOLINT
  ExecuteZsync();
}

TEST_F(Performance, Detect) {  // NOLINT
  // NOTE: the default is artificially low so as not to eat time on the github.
  //       in real life, for performance testing we would want ~30s here.
  size_t target_ms = GetEnv("TEST_TARGET_MS", 1'000);

  data_size = 1'000'000;
  identity_reconstruction = true;

  for (;;) {
    auto beg = Now();
    Execute(false);
    auto end = Now();
    if (DeltaMs(beg, end) > target_ms) {
      break;
    }
    size_t min_factor = 2;
    size_t max_factor = 16;
    data_size *= std::max(
        min_factor,
        std::min(max_factor, target_ms / DeltaMs(beg, end)));
  }

  LOG(INFO) << "detected test size = " << data_size;
}

}  // namespace kysync