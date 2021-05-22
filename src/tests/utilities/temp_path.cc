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

  return root / "tmp" / ("tmp_" + std::to_string(ts) + "_" + std::to_string(counter++));
}

TempPath::TempPath() : TempPath(false, CMAKE_BINARY_DIR) {}

TempPath::TempPath(bool keep, const fs::path &parent_path)
    : keep(keep),
      path(GetUniquePath(parent_path)) {
  CHECK(!fs::exists(path)) << path << " already exists";

  fs::create_directories(path);
  LOG(INFO) << "using " << path;
}

TempPath::~TempPath() {
  if (!keep) {
    std::error_code ec;
    fs::remove_all(path, ec);
    LOG_IF(ERROR, ec) << " " << ec.message();
  }
}

}  // namespace kysync