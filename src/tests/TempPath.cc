#include "TempPath.h"

namespace kysync {

namespace fs = std::filesystem;

struct TempPath::Impl {
  const fs::path path;
};

TempPath::TempPath()
    : impl_(new Impl{.path = fs::temp_directory_path() / "ksync_files_test"}) {
  if (fs::exists(impl_->path)) {
    fs::remove_all(impl_->path);
  }
  fs::create_directories(impl_->path);
}

TempPath::~TempPath() { fs::remove_all(impl_->path); }

std::filesystem::path TempPath::GetPath() const { return impl_->path; }

}  // namespace kysync