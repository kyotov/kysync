#include <glog/logging.h>
#include <gtest/gtest.h>
#include <ky/metrics/metric_callback_visitor.h>
#include <ky/noexcept.h>
#include <ky/observability/observer.h>
#include <ky/temp_path.h>
#include <kysync/commands/prepare_command.h>
#include <kysync/commands/sync_command.h>
#include <kysync/path_config.h>
#include <kysync/test_commands/gen_data_command.h>
#include <kysync/test_commands/system_command.h>
#include <kysync/test_http_servers/http_server.h>
#include <kysync/test_http_servers/nginx_server.h>

#include <chrono>
#include <fstream>

#include "fixture.h"

// with inspiration from
// https://www.sandordargo.com/blog/2019/04/24/parameterized-testing-with-gtest

namespace kysync {

// NOTE: using #X so macro is necessary
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PERFLOG(X) #X << "=" << (X) << std::endl

// TODO(kyotov) discuss... should this be a struct?
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
  bool identity_reconstruction;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  bool http;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  bool zsync;

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
      bool identity_reconstruction,
      bool http,
      bool zsync)
      : tag(std::move(tag)),
        data_size(data_size),
        seed_data_size(seed_data_size),
        fragment_size(fragment_size),
        block_size(block_size),
        blocks_in_batch(blocks_in_batch),
        similarity(similarity),
        threads(threads),
        compression(compression),
        identity_reconstruction(identity_reconstruction),
        http(http),
        zsync(zsync) {}

  PerformanceTestProfile()
      : PerformanceTestProfile(
            Fixture::GetEnv("TEST_TAG", std::string("default")),
            Fixture::GetEnv("TEST_DATA_SIZE", 1'000'000),
            Fixture::GetEnv("TEST_SEED_DATA_SIZE", -1),
            Fixture::GetEnv("TEST_FRAGMENT_SIZE", 123'456),
            Fixture::GetEnv("TEST_BLOCK_SIZE", 16'384),
            Fixture::GetEnvInt("TEST_BLOCKS_IN_BATCH", 4),
            Fixture::GetEnvInt("TEST_SIMILARITY", 90),
            Fixture::GetEnvInt("TEST_THREADS", 32),
            Fixture::GetEnv("TEST_COMPRESSION", false),
            Fixture::GetEnv("TEST_IDENTITY_RECONSTRUCTION", false),
            Fixture::GetEnv("TEST_HTTP", false),
            Fixture::GetEnv("TEST_ZSYNC", false)) {}
};

class PerformanceTestExecution {
  PerformanceTestProfile profile_;
  std::unique_ptr<ky::TempPath> tmp_path_{};
  //  TODO(kyotov): should we provide the old HttpServer as an option?
  //  std::unique_ptr<HttpServer> server_{};
  std::unique_ptr<NginxServer> server_{};
  std::ofstream perf_log_;

  std::filesystem::path root_path_{};
  std::filesystem::path data_file_path_{};
  std::filesystem::path seed_data_file_path_{};
  std::filesystem::path metadata_file_path_{};
  std::filesystem::path compressed_file_path_{};
  std::filesystem::path output_file_path_{};
  std::filesystem::path log_directory_path_{};

  ky::metrics::MetricCallbackVisitor metric_callback_visitor_{};

  void DumpContext() {
    perf_log_ << std::endl                                  //
              << PERFLOG(root_path_)                        //
              << PERFLOG(data_file_path_)                   //
              << PERFLOG(seed_data_file_path_)              //
              << PERFLOG(metadata_file_path_)               //
              << PERFLOG(compressed_file_path_)             //
              << PERFLOG(output_file_path_)                 //
              << PERFLOG(profile_.tag)                      //
              << PERFLOG(profile_.data_size)                //
              << PERFLOG(profile_.seed_data_size)           //
              << PERFLOG(profile_.fragment_size)            //
              << PERFLOG(profile_.block_size)               //
              << PERFLOG(profile_.similarity)               //
              << PERFLOG(profile_.threads)                  //
              << PERFLOG(profile_.compression)              //
              << PERFLOG(profile_.identity_reconstruction)  //
              << PERFLOG(profile_.http)                     //
              << PERFLOG(profile_.zsync);
  }

protected:
  void SnapshotMetrics(ky::metrics::MetricContainer &c) {
    metric_callback_visitor_.Snapshot(
        [this](const std::string &key, ky::metrics::Metric &value) {
          perf_log_ << key << "=" << value << std::endl;
        },
        c);
  }

  void SnapshotMetrics(GenDataCommand &c) {}
  void SnapshotMetrics(SystemCommand &c) {}

  template <typename C>
  void RunAndCollectMetrics(C &c) {
    auto monitor = ky::observability::Observer(c);

    EXPECT_EQ(monitor.Run([&c]() { return c.Run(); }), 0);

    monitor.SnapshotPhases([this](auto key, auto value) {
      perf_log_ << key << "=" << value << std::endl;
    });

    SnapshotMetrics(c);
  }

  [[nodiscard]] const PerformanceTestProfile &GetProfile() const {
    return profile_;
  }

  [[nodiscard]] const std::filesystem::path &GetDataFilePath() const {
    return data_file_path_;
  }

  [[nodiscard]] const std::filesystem::path &GetSeedDataFilePath() const {
    return seed_data_file_path_;
  }

  [[nodiscard]] const std::filesystem::path &GetMetadataFilePath() const {
    return metadata_file_path_;
  }

  [[nodiscard]] const std::filesystem::path &GetCompressedFilePath() const {
    return compressed_file_path_;
  }

  [[nodiscard]] const std::filesystem::path &GetOutputFilePath() const {
    return output_file_path_;
  }

  [[nodiscard]] std::string GetUri(const std::filesystem::path &path) const {
    return profile_.http ? "http://localhost:8000/" + path.filename().string()
                         : "file://" + path.string();
  }

  void GenData() {
    auto gen_data = GenDataCommand(  //
        root_path_,
        profile_.data_size,
        profile_.seed_data_size,
        profile_.fragment_size,
        profile_.similarity);
    RunAndCollectMetrics(gen_data);
  }

  virtual void Prepare() = 0;
  virtual void Sync() = 0;

  virtual bool IsProfileSupported() { return true; };

public:
  explicit PerformanceTestExecution(PerformanceTestProfile profile)
      : profile_(std::move(profile)) {
    tmp_path_ = std::make_unique<ky::TempPath>(
        Fixture::GetEnv("TEST_ROOT_DIR", CMAKE_BINARY_DIR / "tmp"),
        false);
    root_path_ = tmp_path_->GetPath();
    data_file_path_ = root_path_ / "data.bin";
    seed_data_file_path_ = profile_.identity_reconstruction
                               ? data_file_path_
                               : root_path_ / "seed_data.bin";
    metadata_file_path_ = root_path_ / "data.bin.metadata";
    compressed_file_path_ = root_path_ / "data.bin.compressed";
    output_file_path_ = root_path_ / "data.bin.out";
    log_directory_path_ =
        Fixture::GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");

    perf_log_.open(log_directory_path_ / "perf.log", std::ios::app);

    if (profile_.http) {
      //      server_ = std::make_unique<HttpServer>(root_path_, 8000, true);
      server_ = std::make_unique<NginxServer>(root_path_, 8000);
    }

    std::filesystem::current_path(root_path_);
  }

  virtual ~PerformanceTestExecution() {
    ky::NoExcept([this]() {
      // NOTE: we need to tear down the http server before we clean up the path,
      //       because the server is running out of that path.
      if (profile_.http) {
        server_.reset();
      }

      // NOTE: move out of the temp folder as we are about to nuke it!
      std::filesystem::current_path(CMAKE_BINARY_DIR);
      // NOTE: this is not strictly speaking necessary but it is here to
      // emphasize
      //       that the order of cleanup is server first and tmp_path second.
      tmp_path_.reset();
    });
  }

  void Execute() {
    if (IsProfileSupported()) {
      DumpContext();
      GenData();
      Prepare();
      Sync();
    } else {
      LOG(WARNING) << "profile not supported!";
    }
  }
};

class KySyncPerformanceTestExecution final : public PerformanceTestExecution {
  void Prepare() override {
    auto prepare = PrepareCommand::Create(  //
        GetDataFilePath(),
        GetMetadataFilePath(),
        GetCompressedFilePath(),
        GetProfile().block_size,
        GetProfile().threads);
    RunAndCollectMetrics(*prepare);
  }

  void Sync() override {
    auto sync = SyncCommand::Create(  //
        GetUri(GetDataFilePath()),
        GetUri(GetMetadataFilePath()),
        "file://" + GetSeedDataFilePath().string(),  // seed is always local
        GetOutputFilePath(),
        !GetProfile().compression,
        GetProfile().blocks_in_batch,
        GetProfile().threads);
    RunAndCollectMetrics(*sync);
  }

public:
  explicit KySyncPerformanceTestExecution(PerformanceTestProfile profile)
      : PerformanceTestExecution(std::move(profile)) {
    LOG_ASSERT(!GetProfile().zsync);
  }
};

class ZsyncPerformanceTestExecution final : public PerformanceTestExecution {
  std::filesystem::path zsync_bin_path_;

  void Prepare() override {
    std::stringstream command;
    command << zsync_bin_path_ << "/zsyncmake " << GetDataFilePath()  //
            << " -o " << GetMetadataFilePath()                        //
            << " -u data.bin";
    LOG(INFO) << command.str();
    auto prepare = SystemCommand("prepare", command.str());
    RunAndCollectMetrics(prepare);
  }

  void Sync() override {
    std::stringstream command;
    command << zsync_bin_path_ << "/zsync -q "  //
            << " -i " << GetSeedDataFilePath()  //
            << " -o " << GetOutputFilePath()    //
            << " " << GetUri(GetMetadataFilePath());
    LOG(INFO) << command.str();
    auto sync = SystemCommand("sync", command.str());
    RunAndCollectMetrics(sync);
  }

public:
  explicit ZsyncPerformanceTestExecution(PerformanceTestProfile profile)
      : PerformanceTestExecution(std::move(profile)),
        zsync_bin_path_(
            Fixture::GetEnv("ZSYNC_BIN_DIR", CMAKE_BINARY_DIR / "zsync/bin")) {
    LOG_ASSERT(GetProfile().zsync);
  }

  bool IsProfileSupported() override { return ZSYNC_SUPPORTED; }
};

class Performance : public Fixture {
protected:
  static void SetUpTestSuite() {
    Fixture::SetUpTestSuite();
    GetDataSize();
  }

  static std::streamsize GetDataSize() {
    static std::streamsize data_size = -1;

    if (data_size < 0) {
      auto target_ms = GetEnvInt("TEST_LONG_TARGET_MS", 1000);
      data_size = Detect(target_ms);
      LOG(INFO) << "data_size = " << data_size;
    }

    return data_size;
  }

  void SetUp() override {}

  static std::streamsize Detect(intmax_t target_ms) {
    auto profile = PerformanceTestProfile();

    profile.data_size = 1'000'000;
    profile.identity_reconstruction = true;

    for (;;) {
      auto beg = ky::timer::Now();
      GetExecution(profile)->Execute();
      auto end = ky::timer::Now();
      if (ky::timer::DeltaMs(beg, end) > target_ms) {
        break;
      }
      std::streamsize min_factor = 2;
      std::streamsize max_factor = 16;
      profile.data_size *= std::max(
          min_factor,
          std::min(max_factor, target_ms / ky::timer::DeltaMs(beg, end)));
    }

    return profile.data_size;
  }

  static std::unique_ptr<PerformanceTestExecution> GetExecution(
      PerformanceTestProfile &profile) {
    if (profile.zsync) {
      return std::make_unique<ZsyncPerformanceTestExecution>(profile);
    }
    return std::make_unique<KySyncPerformanceTestExecution>(profile);
  }
};

TEST_F(Performance, KySync) {  // NOLINT
  auto profile = PerformanceTestProfile();
  auto execution = GetExecution(profile);
  execution->Execute();
}

TEST_F(Performance, KySync_Http) {  // NOLINT
  auto profile = PerformanceTestProfile();
  profile.http = true;
  auto execution = GetExecution(profile);
  execution->Execute();
}

TEST_F(Performance, Zsync_Http) {  // NOLINT
  auto profile = PerformanceTestProfile();
  profile.http = true;
  // TODO(kyotov): make sure zsync asserts http is on
  profile.zsync = true;
  auto execution = GetExecution(profile);
  execution->Execute();
}

TEST_F(Performance, Detect) {  // NOLINT
  // NOTE: the default is artificially low so as not to eat time on the
  // github.
  //       in real life, for performance testing we would want ~30s here.
  auto target_ms = GetEnvInt("TEST_TARGET_MS", 1'000);
  auto result = Detect(target_ms);
  LOG(INFO) << "detected test size = " << result;
}

TEST_F(Performance, Long) {  // NOLINT
  //  std::map<std::string, PerformanceTestProfile> profiles = {
  //      {"default", PerformanceTestProfile()},
  //      {"perf30s", PerformanceTestProfile()}};
  auto profile = PerformanceTestProfile();
  profile.data_size = GetDataSize();
  auto execution = GetExecution(profile);
  execution->Execute();
}

}  // namespace kysync
