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

#define PERFLOG(X) #X << "=" << X << std::endl

class Performance : public Fixture {
protected:
  std::unique_ptr<TempPath> tmp_path_;
  fs::path root_path_;
  fs::path log_directory_path_;
  fs::path data_file_path_;
  fs::path seed_data_file_path_;
  fs::path metadata_file_path_;
  fs::path compressed_file_path_;
  fs::path output_file_path_;
  size_t data_size_;
  size_t seed_data_size_;
  size_t fragment_size_;
  size_t block_size_;
  int similarity_;
  int blocks_in_batch_;
  int threads_;
  bool compression_;
  bool identity_reconstruction_;
  std::ofstream log_;

  void SetUp() {
    tmp_path_ = std::make_unique<TempPath>(
        GetEnv("TEST_ROOT_DIR", CMAKE_BINARY_DIR / "tmp"),
        false);
    root_path_ = tmp_path_->GetPath();
    log_directory_path_ = GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");
    data_file_path_ = root_path_ / "data.bin";
    seed_data_file_path_ = identity_reconstruction_
                               ? data_file_path_
                               : root_path_ / "seed_data.bin";
    metadata_file_path_ = root_path_ / "data.bin.metadata";
    compressed_file_path_ = root_path_ / "data.bin.compressed";
    output_file_path_ = root_path_ / "data.bin.out";

    data_size_ = GetEnv("TEST_DATA_SIZE", 1'000'000ULL);
    seed_data_size_ = GetEnv("TEST_SEED_DATA_SIZE", -1ULL);
    fragment_size_ = GetEnv("TEST_FRAGMENT_SIZE", 123'456);
    block_size_ = GetEnv("TEST_BLOCK_SIZE", 16'384);
    similarity_ = GetEnv("TEST_SIMILARITY", 90);
    blocks_in_batch_ = GetEnv("TEST_BLOCKS_IN_BATCH", 4);
    threads_ = GetEnv("TEST_THREADS", 32);
    compression_ = GetEnv("TEST_COMPRESSION", false);
    identity_reconstruction_ = GetEnv("TEST_IDENTITY_RECONSTRUCTION", false);

    log_.open(log_directory_path_ / "perf.log", std::ios::app);
    log_ << std::endl                       //
         << PERFLOG(root_path_)             //
         << PERFLOG(data_file_path_)        //
         << PERFLOG(seed_data_file_path_)   //
         << PERFLOG(metadata_file_path_)    //
         << PERFLOG(compressed_file_path_)  //
         << PERFLOG(output_file_path_)      //
         << PERFLOG(data_size_)             //
         << PERFLOG(seed_data_size_)        //
         << PERFLOG(fragment_size_)         //
         << PERFLOG(block_size_)            //
         << PERFLOG(similarity_)            //
         << PERFLOG(threads_)               //
         << PERFLOG(compression_)           //
         << PERFLOG(identity_reconstruction_);

    auto gen_data = GenDataCommand(  //
        root_path_,
        data_size_,
        seed_data_size_,
        fragment_size_,
        similarity_);
    MetricSnapshot(gen_data);
  }

  enum class Http {
    kDontUse,
    kUse,
  };

  enum class Tool { kKySync, kZsync };

  Http http_;
  HttpServer *server_ = nullptr;

  std::string GetUri(const fs::path &path) {
    if (http_ == Http::kUse) {
      return "http://localhost:8000/" + path.filename().string();
    } else {
      return "file://" + path.string();
    }
  }

  void Execute(Http http, Tool tool) {
    auto execute = [&]() {
      switch (tool) {
        case Tool::kKySync:
          ExecuteKySync();
          break;
        case Tool::kZsync:
          ExecuteZsync();
          break;
      };
    };

    http_ = http;
    switch (http_) {
      case Http::kUse: {
        auto server = HttpServer(root_path_, 8000, true);
        server_ = &server;
        execute();
        break;
      }
      case Http::kDontUse: {
        server_ = nullptr;
        execute();
        break;
      }
    }
  }

  void MetricSnapshot(Command &command) {
    auto monitor = Monitor(command);

    if (server_ != nullptr) {
      monitor.RegisterMetricContainer("HttpServer", *server_);
    }

    EXPECT_EQ(monitor.Run(), 0);

    const auto *command_name = typeid(command).name();
    monitor.MetricSnapshot([&](std::string key, auto value) {
      log_ << command_name << ":" << key << "=" << value << std::endl;
    });
  }

  void ExecuteKySync();
  void ExecuteZsync();
};

// FIXME: passing the metric_container is kind of ugly... find a better way
void Performance::ExecuteKySync() {
  auto prepare = PrepareCommand(  //
      data_file_path_,
      metadata_file_path_,
      compressed_file_path_,
      block_size_);
  MetricSnapshot(prepare);

  auto sync = SyncCommand(  //
      GetUri(data_file_path_),
      GetUri(metadata_file_path_),
      "file://" + seed_data_file_path_.string(),  // seed is always local
      output_file_path_,
      !compression_,
      blocks_in_batch_,
      threads_);
  MetricSnapshot(sync);
}

void Performance::ExecuteZsync() {
  auto server = HttpServer(root_path_, 8000, true);

  fs::path zsync_path = GetEnv("ZSYNC_BIN_DIR", CMAKE_BINARY_DIR / "zsync/bin");

  if (!fs::exists(zsync_path / "zsync")) {
    LOG(WARNING)
        << "zsync binary not found, skipping zsync test (are you on Windows!?)";
    return;
  }

  {
    std::stringstream command;
    command << zsync_path << "/zsyncmake " << data_file_path_  //
            << " -o " << metadata_file_path_                   //
            << " -u data.bin";
    LOG(INFO) << command.str();
    auto prepare = SystemCommand(command.str());
    MetricSnapshot(prepare);
  }

  {
    std::stringstream command;
    command << zsync_path << "/zsync -q "      //
            << " -i " << seed_data_file_path_  //
            << " -o " << output_file_path_     //
            << " " << GetUri(metadata_file_path_);
    LOG(INFO) << command.str();
    auto sync = SystemCommand(command.str());
    MetricSnapshot(sync);
  }
}

TEST_F(Performance, KySync) {  // NOLINT
  Execute(Performance::Http::kDontUse, Performance::Tool::kKySync);
}

TEST_F(Performance, KySync_Http) {  // NOLINT
  Execute(Performance::Http::kUse, Performance::Tool::kKySync);
}

TEST_F(Performance, Zsync) {  // NOLINT
  Execute(Performance::Http::kDontUse, Performance::Tool::kZsync);
}

TEST_F(Performance, Zsync_Http) {  // NOLINT
  Execute(Performance::Http::kUse, Performance::Tool::kZsync);
}

TEST_F(Performance, Detect) {  // NOLINT
  // NOTE: the default is artificially low so as not to eat time on the github.
  //       in real life, for performance testing we would want ~30s here.
  size_t target_ms = GetEnv("TEST_TARGET_MS", 1'000);

  data_size_ = 1'000'000;
  identity_reconstruction_ = true;

  for (;;) {
    auto beg = Now();
    Execute(Performance::Http::kDontUse, Performance::Tool::kKySync);
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