#include "TempPath.h"

namespace kysync {

namespace fs = std::filesystem;

struct TempPath::Impl {
  const bool kKeep;
  const fs::path kPath;
};

TempPath::TempPath(bool keep)
    : impl_(new Impl{
          .kKeep = keep,
          .kPath = fs::temp_directory_path() / "ksync_files_test"}) {
  if (fs::exists(impl_->kPath)) {
    fs::remove_all(impl_->kPath);
  }
  fs::create_directories(impl_->kPath);
}

TempPath::~TempPath() {
  if (!impl_->kKeep) {
    fs::remove_all(impl_->kPath);
  }
}

std::filesystem::path TempPath::GetPath() const { return impl_->kPath; }

}  // namespace kysync