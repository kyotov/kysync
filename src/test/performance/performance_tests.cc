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
#include <kysync/test_common/test_fixture.h>
#include <kysync/test_common/test_environment.h>
#include <kysync/test_http_servers/http_server.h>
#include <kysync/test_http_servers/nginx_server.h>

#include <chrono>
#include <fstream>

#include "performance_test_fixture.h"
#include "performance_test_profile.h"

// with inspiration from
// https://www.sandordargo.com/blog/2019/04/24/parameterized-testing-with-gtest

namespace kysync {

TestEnvironment *test_environment = dynamic_cast<TestEnvironment *>(
    testing::AddGlobalTestEnvironment(new TestEnvironment()));

// NOTE: using #X so macro is necessary
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PERFLOG(X) #X << "=" << (X) << std::endl

class PerformanceTestExecution {
  PerformanceTestProfile profile_;
  //  TODO(kyotov): should we provide the old HttpServer as an option?
  //  std::unique_ptr<HttpServer> server_{};
  std::unique_ptr<NginxServer> server_{};

  std::filesystem::path data_file_path_{};
  std::filesystem::path seed_data_file_path_{};
  std::filesystem::path metadata_file_path_{};
  std::filesystem::path compressed_file_path_{};
  std::filesystem::path output_file_path_{};

  ky::metrics::MetricCallbackVisitor metric_callback_visitor_{};

  std::ofstream &perf_log_;
  std::filesystem::path scratch_path_;

  void DumpContext() {
    perf_log_ << std::endl                         //
              << PERFLOG(data_file_path_)          //
              << PERFLOG(seed_data_file_path_)     //
              << PERFLOG(metadata_file_path_)      //
              << PERFLOG(compressed_file_path_)    //
              << PERFLOG(output_file_path_)        //
              << PERFLOG(profile_.tag)             //
              << PERFLOG(profile_.data_size)       //
              << PERFLOG(profile_.seed_data_size)  //
              << PERFLOG(profile_.fragment_size)   //
              << PERFLOG(profile_.block_size)      //
              << PERFLOG(profile_.similarity)      //
              << PERFLOG(profile_.threads)         //
              << PERFLOG(profile_.compression)     //
              << PERFLOG(profile_.http)            //
              << PERFLOG(profile_.zsync)           //
              << PERFLOG(profile_.flush_caches);
  }

protected:
  void RunAndCollectMetrics(Command &c) {
    auto monitor = ky::observability::Observer(c);

    EXPECT_EQ(monitor.Run([&c]() { return c.Run(); }), 0);

    monitor.SnapshotPhases([this](auto key, auto value) {
      perf_log_ << key << "=" << value << std::endl;
    });

    metric_callback_visitor_.Snapshot(
        [this](const std::string &key, ky::metrics::Metric &value) {
          perf_log_ << key << "=" << value << std::endl;
        },
        c);
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
        scratch_path_,
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
  explicit PerformanceTestExecution(
      PerformanceTestProfile profile,
      std::filesystem::path scratch_path)
      : profile_(std::move(profile)),
        perf_log_(test_environment->GetPerfLog()),
        scratch_path_(std::move(scratch_path)) {
    data_file_path_ = scratch_path_ / "data.bin";
    seed_data_file_path_ = scratch_path_ / "seed_data.bin";
    metadata_file_path_ = scratch_path_ / "data.bin.metadata";
    compressed_file_path_ = scratch_path_ / "data.bin.compressed";
    output_file_path_ = scratch_path_ / "data.bin.out";

    if (profile_.http) {
      //      server_ = std::make_unique<HttpServer>(root_path_, 8000, true);
      server_ = std::make_unique<NginxServer>(scratch_path_, 8000);
    }

    //    std::filesystem::current_path(scratch_path_);
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
    });
  }

  void Execute() {
    if (IsProfileSupported()) {
      DumpContext();
      GenData();
      PerformanceTestFixture::FlushCaches();
      Prepare();
      PerformanceTestFixture::FlushCaches();
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
  explicit KySyncPerformanceTestExecution(
      PerformanceTestProfile profile,
      std::filesystem::path scratch_path)
      : PerformanceTestExecution(std::move(profile), std::move(scratch_path)) {
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
  explicit ZsyncPerformanceTestExecution(
      PerformanceTestProfile profile,
      std::filesystem::path scratch_path)
      : PerformanceTestExecution(std::move(profile), std::move(scratch_path)),
        zsync_bin_path_(TestEnvironment::GetEnv(
            "ZSYNC_BIN_DIR",
            CMAKE_BINARY_DIR / "zsync/bin")) {
    LOG_ASSERT(GetProfile().zsync);
  }

  bool IsProfileSupported() override { return ZSYNC_SUPPORTED; }
};

class Performance : public PerformanceTestFixture {
protected:
  std::unique_ptr<PerformanceTestExecution> GetExecution(
      PerformanceTestProfile &profile) {
    auto tmp = GetScratchPath();
    if (profile.zsync) {
      return std::make_unique<ZsyncPerformanceTestExecution>(profile, tmp);
    } else {
      return std::make_unique<KySyncPerformanceTestExecution>(profile, tmp);
    }
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

}  // namespace kysync
