#include "temp_path.h"

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

std::string GetCurrentNanosAsString() {
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  return std::to_string(
      duration_cast<nanoseconds>(now.time_since_epoch()).count());
}

TempPath::TempPath() : TempPath(false, std::filesystem::temp_directory_path()) {
  // No-op
}

TempPath::TempPath(bool keep, const fs::path &path) {
  keep_ = keep;
  full_path_ = path / ("kysync_test_" + GetCurrentNanosAsString() + "_" +
                       std::to_string(counter_.fetch_add(1)));

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