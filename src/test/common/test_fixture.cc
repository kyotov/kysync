#include <glog/logging.h>
#include <kysync/path_config.h>
#include <kysync/test_common/test_environment.h>
#include <kysync/test_common/test_fixture.h>

namespace kysync {

Fixture::Fixture() : temp_path_(TestEnvironment::GetTmpRoot(), false) {
  LOG(INFO) << "tmp = " << temp_path_.GetPath();
}

std::filesystem::path Fixture::GetScratchPath() const {
  return temp_path_.GetPath();
}

}  // namespace kysync