#include "temp_path.h"

#include <kysync/path_config.h>

#include <atomic>

#include "glog/logging.h"

namespace kysync {

namespace fs = std::filesystem;

static fs::path GetUniquePath(const fs::path &root) {
  static std::atomic<uint32_t> counter = 0;
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  auto ts = duration_cast<nanoseconds>(now.time_since_epoch()).count();

  return root / ("tmp_" + std::to_string(ts) + "_" + std::to_string(counter++));
}

TempPath::TempPath() : TempPath(CMAKE_BINARY_DIR / "tmp", false){};

TempPath::TempPath(const fs::path &parent_path, bool keep)
    : kPath(GetUniquePath(parent_path)),
      kKeep(keep) {
  CHECK(!fs::exists(kPath)) << "temporary path " << kPath << "already exists";
  fs::create_directories(kPath);
  LOG(INFO) << "using temporary path " << kPath;
}

TempPath::~TempPath() {
  if (!kKeep) {
    std::error_code ec;
    fs::remove_all(kPath, ec);
    LOG_IF(ERROR, !ec) << ec.message();
  }
}

std::filesystem::path TempPath::GetPath() const { return kPath; }

}  // namespace kysync