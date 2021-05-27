#ifndef KSYNC_TEMP_PATH_H
#define KSYNC_TEMP_PATH_H

#include <filesystem>

namespace kysync {

class TempPath final {
  std::filesystem::path path_;
  bool keep_;

public:
  TempPath();

  TempPath(const std::filesystem::path &parent_path, bool keep);

  ~TempPath();

  [[nodiscard]] std::filesystem::path GetPath() const;
};

}  // namespace kysync

#endif  // KSYNC_TEMP_PATH_H
