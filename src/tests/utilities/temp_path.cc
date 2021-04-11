#include "temp_path.h"

#include "glog/logging.h"
#include <cstdlib>

namespace kysync {

namespace fs = std::filesystem;

struct TempPath::Impl {
  const bool kKeep;
  const fs::path kRootPath;
  fs::path temp_path_;
};

TempPath::TempPath(bool keep, const std::filesystem::path root)
    : impl_(new Impl{.kKeep = keep, .kRootPath = root}) {
  char dir_name_template[] = "ksync_files_test_XXXXXX";
  impl_->temp_path_ = root / mkdtemp(dir_name_template);
  CHECK(!fs::exists(impl_->temp_path_))
      << "Unexpected existence of temp dir " << impl_->temp_path_;
  fs::create_directories(impl_->temp_path_);
}

TempPath::~TempPath() {
  if (!impl_->kKeep) {
    fs::remove_all(impl_->temp_path_);
  }
}

std::filesystem::path TempPath::GetPath() const { return impl_->temp_path_; }

}  // namespace kysync