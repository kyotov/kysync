#include <glog/logging.h>
#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

#include "../commands/prepare_command.h"
#include "../commands/sync_command.h"
#include "../monitor.h"
#include "../utilities/timer.h"
#include "commands/gen_data_command.h"
#include "http/http_server.h"
#include "utilities/temp_path.h"

// with inspiration from
// https://www.sandordargo.com/blog/2019/04/24/parameterized-testing-with-gtest

namespace kysync {

template <typename T>
T GetEnv(const std::string &name, T default_value) {
  auto env = std::getenv(name.c_str());
  errno = 0;
  auto value = env == nullptr ? default_value
                              : static_cast<T>(strtoumax(env, nullptr, 10));
  value = errno ? default_value : value;
  LOG(INFO) << name << "=" << value;
  return value;
}

struct Context {
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

  Context(const Context &) = default;

  static Context Default() {
    static const Context value{};
    return value;
  }

private:
  Context() {
    auto root_path = std::getenv("TEST_ROOT_PATH");
    path = root_path != nullptr ? root_path
                                : std::filesystem::temp_directory_path();
    LOG(INFO) << "TEST_ROOT_PATH=" << path;

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
void Execute(
    const Context &context,
    const TempPath &tmp,
    MetricContainer *metric_container,
    GetUriCallback get_uri_callback) {
  const fs::path kRootPath = tmp.GetPath();
  const fs::path kDataPath = kRootPath / "data.bin";
  const fs::path kSeedDataPath =
      context.identity_reconstruction ? kDataPath : kRootPath / "seed_data.bin";
  const fs::path kKysyncPath = kRootPath / "data.bin.kysync";
  const fs::path kPzstPath = kRootPath / "data.bin.pzst";
  const fs::path kOutPath = kRootPath / "data.bin.out";

  std::ofstream log("perf.log", std::ios::app);

  log << std::endl                        //
      << PERFLOG(context.path)            //
      << PERFLOG(context.data_size)       //
      << PERFLOG(context.seed_data_size)  //
      << PERFLOG(context.fragment_size)   //
      << PERFLOG(context.block_size)      //
      << PERFLOG(context.similarity)      //
      << PERFLOG(context.threads)         //
      << PERFLOG(context.compression)     //
      << PERFLOG(context.identity_reconstruction);

  auto gen_data = GenDataCommand(  //
      tmp.GetPath(),
      context.data_size,
      context.seed_data_size,
      context.fragment_size,
      context.similarity);
  MetricSnapshot(log, gen_data, nullptr);

  auto prepare = PrepareCommand(  //
      kDataPath,
      kKysyncPath,
      kPzstPath,
      context.block_size);
  MetricSnapshot(log, prepare, nullptr);

  auto sync = SyncCommand(  //
      get_uri_callback(kDataPath),
      get_uri_callback(kKysyncPath),
      "file://" + kSeedDataPath.string(), // seed is always local
      kOutPath,
      !context.compression,
      context.num_blocks_in_batch,
      context.threads);
  MetricSnapshot(log, sync, metric_container);
}

void Execute(const Context &context, bool use_http) {
  const auto tmp = TempPath(false, context.path);

  if (use_http) {
    auto server = HttpServer(tmp.GetPath(), 8000);
    Execute(context, tmp, &server, [](auto path) {
      return "http://localhost:8000/" + path.filename().string();
    });
  } else {
    Execute(context, tmp, nullptr, [](auto path) {
      return "file://" + path.string();
    });
  }
}

TEST(Performance, Simple) {  // NOLINT
  auto ctx = Context::Default();
  Execute(ctx, false);
}

TEST(Performance, Http) {  // NOLINT
  auto ctx = Context::Default();
  Execute(ctx, true);
}

TEST(Performance, Detect) {  // NOLINT
  // NOTE: the default is artificially low so as not to eat time on the github.
  //       in real life, for performance testing we would want ~30s here.
  size_t target_ms = GetEnv("TEST_TARGET_MS", 1'000);

  auto ctx = Context::Default();
  ctx.data_size = 1'000'000;
  ctx.identity_reconstruction = true;

  for (;;) {
    auto beg = Now();
    Execute(ctx, false);
    auto end = Now();
    if (DeltaMs(beg, end) > target_ms) {
      break;
    }
    size_t min_factor = 2;
    size_t max_factor = 16;
    ctx.data_size *= std::max(
        min_factor,
        std::min(max_factor, target_ms / DeltaMs(beg, end)));
  }

  LOG(INFO) << "detected test size = " << ctx.data_size;
}

}  // namespace kysync