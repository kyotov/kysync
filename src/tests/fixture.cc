#include "fixture.h"

#include <glog/logging.h>
#include <kysync/path_config.h>

#include "test_environment.h"

namespace kysync {

Fixture::Fixture() : temp_path_(TestEnvironment::GetTmpRoot(), false) {
  LOG(INFO) << "tmp = " << temp_path_.GetPath();
}

std::filesystem::path Fixture::GetScratchPath() const {
  return temp_path_.GetPath();
}

}  // namespace kysync