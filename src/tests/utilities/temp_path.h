#ifndef KSYNC_TEMP_PATH_H
#define KSYNC_TEMP_PATH_H

#include <atomic>
#include <filesystem>

#include "../../utilities/utilities.h"

namespace kysync {

class TempPath {
  NO_COPY_OR_MOVE(TempPath);

public:
  explicit TempPath();
  explicit TempPath(bool keep, const std::filesystem::path &path);
  ~TempPath();
  [[nodiscard]] std::filesystem::path GetPath() const;

private:
  bool keep_;
  std::filesystem::path full_path_;
  static std::atomic<uint32_t> counter_;
};

}  // namespace kysync

#endif  // KSYNC_TEMP_PATH_H
