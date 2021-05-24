#include "fixture.h"

#include <glog/logging.h>
#include <kysync/path_config.h>

namespace kysync {

bool Fixture::glog_initialized_ = false;

void Fixture::SetUpTestSuite() {
  if (!glog_initialized_) {
    glog_initialized_ = true;

    FLAGS_log_dir = GetEnv("TEST_LOG_DIR", (CMAKE_BINARY_DIR / "log").string());
    google::InitGoogleLogging("tests");

    FLAGS_logtostderr = false;
    FLAGS_alsologtostderr = false;
  }
}

std::string Fixture::GetEnv(
    const std::string& name,
    const std::string& default_value) {
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

uintmax_t Fixture::GetEnv(const std::string& name, uintmax_t default_value) {
  auto str_value = GetEnv(name, std::to_string(default_value));
  return std::stoull(str_value, nullptr, 10);
}

}  // namespace kysync