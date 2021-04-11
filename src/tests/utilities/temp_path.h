#ifndef KSYNC_TEMP_PATH_H
#define KSYNC_TEMP_PATH_H

#include <filesystem>

#include "../../utilities/utilities.h"

namespace kysync {

class TempPath {
  PIMPL;
  NO_COPY_OR_MOVE(TempPath);

public:
  explicit TempPath(
      bool keep = false,
      std::filesystem::path root = std::filesystem::temp_directory_path());

  ~TempPath();

  [[nodiscard]] std::filesystem::path GetPath() const;
};

}  // namespace kysync

#endif  // KSYNC_TEMP_PATH_H
