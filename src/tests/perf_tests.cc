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
  fs::path path_;
  size_t data_size_;
  size_t seed_data_size_;
  size_t fragment_size_;
  size_t block_size_;
  int similarity_;
  int num_blocks_in_batch_;
  int threads_;
  bool compression_;
  bool identity_reconstruction_;

  Performance() {
    auto root_path = GetEnv("TEST_ROOT_DIR", CMAKE_BINARY_DIR);
    data_size_ = GetEnv("TEST_DATA_SIZE", 1'000'000ULL);
    seed_data_size_ = GetEnv("TEST_SEED_DATA_SIZE", -1ULL);
    fragment_size_ = GetEnv("TEST_FRAGMENT_SIZE", 123'456);
    block_size_ = GetEnv("TEST_BLOCK_SIZE", 16'384);
    similarity_ = GetEnv("TEST_SIMILARITY", 90);
    num_blocks_in_batch_ = GetEnv("TEST_NUM_BLOCKS_IN_BATCH", 4);
    threads_ = GetEnv("TEST_THREADS", 32);
    compression_ = GetEnv("TEST_COMPRESSION", false);
    identity_reconstruction_ = GetEnv("TEST_IDENTITY_RECONSTRUCTION", false);
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
  const fs::path kRootPath = tmp.path;
  const fs::path kDataPath = kRootPath / "data.bin";
  const fs::path kSeedDataPath =
      identity_reconstruction_ ? kDataPath : kRootPath / "seed_data.bin";
  const fs::path kKysyncPath = kRootPath / "data.bin.kysync";
  const fs::path kPzstPath = kRootPath / "data.bin.pzst";
  const fs::path kOutPath = kRootPath / "data.bin.out";
  const fs::path kLogPath = GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");

  std::ofstream log(kLogPath / "perf.log", std::ios::app);

  log << std::endl                //
      << PERFLOG(path_)            //
      << PERFLOG(data_size_)       //
      << PERFLOG(seed_data_size_)  //
      << PERFLOG(fragment_size_)   //
      << PERFLOG(block_size_)      //
      << PERFLOG(similarity_)      //
      << PERFLOG(threads_)         //
      << PERFLOG(compression_)     //
      << PERFLOG(identity_reconstruction_);

  auto gen_data = GenDataCommand(  //
      tmp.path,
      data_size_,
      seed_data_size_,
      fragment_size_,
      similarity_);
  MetricSnapshot(log, gen_data, nullptr);

  auto prepare = PrepareCommand(  //
      kDataPath,
      kKysyncPath,
      kPzstPath,
      block_size_);
  MetricSnapshot(log, prepare, nullptr);

  auto sync = SyncCommand(  //
      get_uri_callback(kDataPath),
      get_uri_callback(kKysyncPath),
      "file://" + kSeedDataPath.string(),  // seed is always local
      kOutPath,
      !compression_,
      num_blocks_in_batch_,
      threads_);
  MetricSnapshot(log, sync, metric_container);
}

void Performance::Execute(bool use_http) {
  const auto tmp = TempPath(false, path_);

  if (use_http) {
    auto server = HttpServer(tmp.path, 8000, true);
    Execute(tmp, &server, [](auto path) {
      return "http://localhost:8000/" + path.filename().string();
    });
  } else {
    Execute(tmp, nullptr, [](auto path) { return "file://" + path.string(); });
  }
}

void Performance::ExecuteZsync() {
  const auto tmp = TempPath(true, path_);
  auto server = HttpServer(tmp.path, 8000, true);

  const fs::path kRootPath = tmp.path;
  const fs::path kDataPath = kRootPath / "data.bin";
  const fs::path kSeedDataPath =
      identity_reconstruction_ ? kDataPath : kRootPath / "seed_data.bin";
  const fs::path kZsyncPath = kRootPath / "data.bin.kysync";
  const fs::path kPzstPath = kRootPath / "data.bin.pzst";
  const fs::path kOutPath = kRootPath / "data.bin.out";

  std::ofstream log("perf.log", std::ios::app);

  log << std::endl                //
      << PERFLOG(path_)            //
      << PERFLOG(data_size_)       //
      << PERFLOG(seed_data_size_)  //
      << PERFLOG(fragment_size_)   //
      << PERFLOG(block_size_)      //
      << PERFLOG(similarity_)      //
      << PERFLOG(threads_)         //
      << PERFLOG(compression_)     //
      << PERFLOG(identity_reconstruction_);

  const fs::path zsync_path = GetEnv("ZSYNC_BIN_DIR", CMAKE_BINARY_DIR / "zsync/bin");

  if (!fs::exists(zsync_path / "zsync")) {
    LOG(WARNING) << "zsync binary not found, skipping zsync test (are you on Windows!?)";
    return;
  }

  auto gen_data = GenDataCommand(  //
      tmp.path,
      data_size_,
      seed_data_size_,
      fragment_size_,
      similarity_);
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

  data_size_ = 1'000'000;
  identity_reconstruction_ = true;

  for (;;) {
    auto beg = Now();
    Execute(false);
    auto end = Now();
    if (DeltaMs(beg, end) > target_ms) {
      break;
    }
    size_t min_factor = 2;
    size_t max_factor = 16;
    data_size_ *= std::max(
        min_factor,
        std::min(max_factor, target_ms / DeltaMs(beg, end)));
  }

  LOG(INFO) << "detected test size = " << data_size_;
}

}  // namespace kysync
