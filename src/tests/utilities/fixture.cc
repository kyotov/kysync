#include "fixture.h"

#include <glog/logging.h>

namespace kysync {

void Fixture::SetUpTestSuite() {
  google::InitGoogleLogging("tests");

  FLAGS_log_dir = std::getenv("TEST_LOG_DIR");
  FLAGS_logtostderr = false;
  FLAGS_alsologtostderr = false;
}

std::string Fixture::GetEnv(
    const std::string& name,
    const std::string& default_value) {
  auto *env = std::getenv(name.c_str());
  auto value = env == nullptr ? default_value : env;
  LOG(INFO) << name << "=" << value;
  return value;
}

uintmax_t Fixture::GetEnv(const std::string& name, uintmax_t default_value) {
  auto str_value = GetEnv(name, std::to_string(default_value));
  return std::stoull(str_value, nullptr, 10);
}

}  // namespace kysync