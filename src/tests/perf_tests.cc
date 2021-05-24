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
  int blocks_in_batch_;
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
    blocks_in_batch_ = GetEnv("TEST_BLOCKS_IN_BATCH", 4);
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
  fs::path root_path = tmp.GetPath();
  fs::path data_file_path = root_path / "data.bin";
  fs::path seed_data_file_path =
      identity_reconstruction_ ? data_file_path : root_path / "seed_data.bin";
  fs::path kysync_file_path = root_path / "data.bin.kysync";
  fs::path compressed_file_path = root_path / "data.bin.pzst";
  fs::path output_file_path = root_path / "data.bin.out";
  fs::path log_directory_path =
      GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");

  std::ofstream log(log_directory_path / "perf.log", std::ios::app);

  log << std::endl                 //
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
      tmp.GetPath(),
      data_size_,
      seed_data_size_,
      fragment_size_,
      similarity_);
  MetricSnapshot(log, gen_data, nullptr);

  auto prepare = PrepareCommand(  //
      data_file_path,
      kysync_file_path,
      compressed_file_path,
      block_size_);
  MetricSnapshot(log, prepare, nullptr);

  auto sync = SyncCommand(  //
      get_uri_callback(data_file_path),
      get_uri_callback(kysync_file_path),
      "file://" + seed_data_file_path.string(),  // seed is always local
      output_file_path,
      !compression_,
      blocks_in_batch_,
      threads_);
  MetricSnapshot(log, sync, metric_container);
}

void Performance::Execute(bool use_http) {
  auto tmp = TempPath(path_, false);

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
  auto tmp = TempPath(path_, true);
  auto server = HttpServer(tmp.GetPath(), 8000, true);

  fs::path root_path = tmp.GetPath();
  fs::path data_file_path = root_path / "data.bin";
  fs::path seed_data_file_path =
      identity_reconstruction_ ? data_file_path : root_path / "seed_data.bin";
  fs::path zsync_file_path = root_path / "data.bin.kysync";
  fs::path output_file_path = root_path / "data.bin.out";
  fs::path log_directory_path =
      GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");

  std::ofstream log(log_directory_path / "perf.log", std::ios::app);

  log << std::endl                 //
      << PERFLOG(path_)            //
      << PERFLOG(data_size_)       //
      << PERFLOG(seed_data_size_)  //
      << PERFLOG(fragment_size_)   //
      << PERFLOG(block_size_)      //
      << PERFLOG(similarity_)      //
      << PERFLOG(threads_)         //
      << PERFLOG(compression_)     //
      << PERFLOG(identity_reconstruction_);

  fs::path zsync_path = GetEnv("ZSYNC_BIN_DIR", CMAKE_BINARY_DIR / "zsync/bin");

  if (!fs::exists(zsync_path / "zsync")) {
    LOG(WARNING)
        << "zsync binary not found, skipping zsync test (are you on Windows!?)";
    return;
  }

  auto gen_data = GenDataCommand(  //
      tmp.GetPath(),
      data_size_,
      seed_data_size_,
      fragment_size_,
      similarity_);
  MetricSnapshot(log, gen_data, &server);

  {
    std::stringstream command;
    command << zsync_path << "/zsyncmake " << data_file_path  //
            << " -o " << root_path / "data.bin.zsync"         //
            << " -u data.bin";
    LOG(INFO) << command.str();
    auto prepare = SystemCommand(command.str());
    MetricSnapshot(log, prepare, &server);
  }

  {
    std::stringstream command;
    command << zsync_path << "/zsync -q"             //
            << " -i " << seed_data_file_path         //
            << " -o " << root_path / "data.bin.out"  //
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