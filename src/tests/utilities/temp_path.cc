#include "temp_path.h"

#include <kysync/path_config.h>

#include <atomic>

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

static fs::path GetUniquePath(const fs::path &root) {
  using namespace std::chrono;

  static std::atomic<uint32_t> counter = 0;
  auto now = high_resolution_clock::now();
  auto ts = duration_cast<nanoseconds>(now.time_since_epoch()).count();

  return root / ("tmp_" + std::to_string(ts) + "_" + std::to_string(counter++));
}

TempPath::TempPath() : TempPath(CMAKE_BINARY_DIR / "tmp", false) {}

TempPath::TempPath(const fs::path &parent_path, bool keep)
    : path_(GetUniquePath(parent_path)),
      keep_(keep)  //
{
  CHECK(!fs::exists(path_)) << path_ << " already exists";

  fs::create_directories(path_);
  LOG(INFO) << "using " << path_;
}

TempPath::~TempPath() {
  if (!keep_) {
    std::error_code ec;
    fs::remove_all(path_, ec);
    LOG_IF(ERROR, ec) << " " << ec.message();
  }
}

std::filesystem::path TempPath::GetPath() const {
  return path_;
}

}  // namespace kysync