#include "temp_path.h"

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

struct TempPath::Impl {
  const bool kKeep;
  const fs::path kPath;
};

static fs::path GetUniquePath(const fs::path &root) {
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  auto ts = duration_cast<nanoseconds>(now.time_since_epoch()).count();

  return root / ("kysync_test_" + std::to_string(ts));
}

TempPath::TempPath(bool keep, const fs::path &path)
    : impl_(new Impl{.kKeep = keep, .kPath = GetUniquePath(path)}) {
  CHECK(!fs::exists(impl_->kPath))
      << "temporary path " << impl_->kPath << "already exists";
  fs::create_directories(impl_->kPath);
  LOG(INFO) << "using temporary path " << impl_->kPath;
}

TempPath::~TempPath() {
  if (!impl_->kKeep) {
    fs::remove_all(impl_->kPath);
  }
}

std::filesystem::path TempPath::GetPath() const { return impl_->kPath; }

}  // namespace kysync