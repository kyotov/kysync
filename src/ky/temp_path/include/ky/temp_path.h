#ifndef KSYNC_SRC_KY_TEMP_PATH_TEMP_PATH_H
#define KSYNC_SRC_KY_TEMP_PATH_TEMP_PATH_H

#include <filesystem>

namespace ky {

class TempPath final {
  std::filesystem::path path_;
  bool keep_;

public:
  TempPath();

  TempPath(const std::filesystem::path &parent_path, bool keep);

  ~TempPath();

  [[nodiscard]] std::filesystem::path GetPath() const;
};

}  // namespace ky

#endif  // KSYNC_SRC_KY_TEMP_PATH_TEMP_PATH_H
