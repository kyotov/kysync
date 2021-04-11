#include "temp_path.h"

#include <cstdlib>

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

struct TempPath::Impl {
  const bool kKeep;
  const fs::path kPath;
};

TempPath::TempPath(bool keep, const std::filesystem::path &path)
    : impl_(new Impl{.kKeep = keep, .kPath = path / tmpnam(nullptr)}) {
  CHECK(!fs::exists(impl_->kPath))
      << "temporary path " << impl_->kPath << "already exists";
  fs::create_directories(impl_->kPath);
}

TempPath::~TempPath() {
  if (!impl_->kKeep) {
    fs::remove_all(impl_->kPath);
  }
}

std::filesystem::path TempPath::GetPath() const { return impl_->kPath; }

}  // namespace kysync