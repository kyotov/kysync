#include "temp_path.h"

#include <atomic>

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

class TempPath::Impl {
public:
  Impl(bool keep, const fs::path path) : kKeep(keep), kPath(path) {}

  static fs::path GetUniquePath(const fs::path &root);

  const bool kKeep;
  const fs::path kPath;  
};

fs::path TempPath::Impl::GetUniquePath(const fs::path &root) {
  static std::atomic<uint32_t> counter = 0;
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  auto ts = duration_cast<nanoseconds>(now.time_since_epoch()).count();

  return root / ("kysync_test_" + std::to_string(ts) + "_" +
                 std::to_string(counter++));
}

TempPath::TempPath(bool keep, const fs::path &path)
    : impl_(std::make_unique<TempPath::Impl>(
          keep,
          TempPath::Impl::GetUniquePath(path))) {
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