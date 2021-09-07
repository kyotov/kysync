#include <glog/logging.h>
#include <kysync/path_config.h>
#include <kysync/test_common/test_environment.h>

namespace kysync {

TestEnvironment::~TestEnvironment() = default;

void TestEnvironment::SetUp() {
  Environment::SetUp();

  auto log_dir = GetEnv("TEST_LOG_DIR", CMAKE_BINARY_DIR / "log");
  perf_log_.open(log_dir / "perf.log", std::ios::app);

  static bool logging_initialized = false;

  if (!logging_initialized) {
    FLAGS_log_dir = log_dir.string();
    google::InitGoogleLogging("tests");

    FLAGS_logtostderr = false;
    FLAGS_alsologtostderr = false;

    logging_initialized = true;
  }
}

void TestEnvironment::TearDown() {
  perf_log_.close();

  Environment::TearDown();
}

std::ofstream& TestEnvironment::GetPerfLog() { return perf_log_; }

std::filesystem::path TestEnvironment::GetTmpRoot() {
  return TestEnvironment::GetEnv("TEST_ROOT_DIR", CMAKE_BINARY_DIR / "tmp");
}

std::string TestEnvironment::GetEnv(
    const std::string& name,
    const std::string& default_value) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe,clang-diagnostic-deprecated-declarations)
  auto* env = std::getenv(name.c_str());
  auto value = env == nullptr ? default_value : env;
  LOG(INFO) << name << "=" << value;
  return value;
}

std::filesystem::path TestEnvironment::GetEnv(
    const std::string& name,
    const std::filesystem::path& default_value) {
  return GetEnv(name, default_value.string());
}

intmax_t TestEnvironment::GetEnv(
    const std::string& name,
    intmax_t default_value) {
  auto str_value = GetEnv(name, std::to_string(default_value));
  return std::stoll(str_value, nullptr, 10);
}

int TestEnvironment::GetEnvInt(const std::string& name, int default_value) {
  auto result = GetEnv(name, default_value);
  CHECK_LE(result, std::numeric_limits<int>::max());
  CHECK_GE(result, std::numeric_limits<int>::min());
  return static_cast<int>(result);
}

}  // namespace kysync
