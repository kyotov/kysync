#include "temp_path.h"

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

TempPath::TempPath() : TempPath(false, std::filesystem::temp_directory_path()) {
  // No-op
}

std::atomic<uint32_t> TempPath::counter_ = 0;

TempPath::TempPath(bool keep, const fs::path &path) {
  keep_ = keep;
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  auto ts = duration_cast<nanoseconds>(now.time_since_epoch()).count();

  full_path_ =
      path / ("kysync_test_" + std::to_string(ts) + "_" + std::to_string(counter_.fetch_add(1)));

  CHECK(!fs::exists(full_path_))
      << "temporary path " << full_path_ << "already exists";
  fs::create_directories(full_path_);
}

TempPath::~TempPath() {
  if (!keep_) {
    fs::remove_all(full_path_);
  }
}

std::filesystem::path TempPath::GetPath() const { return full_path_; }

}  // namespace kysync