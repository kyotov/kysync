#ifndef KSYNC_TEMP_PATH_H
#define KSYNC_TEMP_PATH_H

#include <filesystem>

#include "../../utilities/utilities.h"

namespace kysync {

class TempPath final {
public:
  explicit TempPath(
      bool keep = false,
      const std::filesystem::path &parent_path =
          std::filesystem::temp_directory_path());

  ~TempPath();

  const bool keep;
  const std::filesystem::path path;
};

}  // namespace kysync

#endif  // KSYNC_TEMP_PATH_H
