#include "fixture.h"

#include <glog/logging.h>
#include <kysync/path_config.h>

namespace kysync {

void Fixture::SetUpTestSuite() {
  static bool glog_initialized = false;
  if (!glog_initialized) {
    FLAGS_log_dir = GetEnv("TEST_LOG_DIR", (CMAKE_BINARY_DIR / "log").string());
    google::InitGoogleLogging("tests");

    FLAGS_logtostderr = false;
    FLAGS_alsologtostderr = false;

    glog_initialized = true;
  }
}

std::string Fixture::GetEnv(
    const std::string& name,
    const std::string& default_value) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe,clang-diagnostic-deprecated-declarations)
  auto* env = std::getenv(name.c_str());
  auto value = env == nullptr ? default_value : env;
  LOG(INFO) << name << "=" << value;
  return value;
}

std::filesystem::path Fixture::GetEnv(
    const std::string& name,
    const std::filesystem::path& default_value) {
  return GetEnv(name, default_value.string());
}

intmax_t Fixture::GetEnv(const std::string& name, intmax_t default_value) {
  auto str_value = GetEnv(name, std::to_string(default_value));
  return std::stoll(str_value, nullptr, 10);
}

int Fixture::GetEnvInt(const std::string& name, int default_value) {
  auto result = Fixture::GetEnv(name, default_value);
  CHECK_LE(result, std::numeric_limits<int>::max());
  CHECK_GE(result, std::numeric_limits<int>::min());
  return static_cast<int>(result);
}

}  // namespace kysync