#ifndef KSYNC_TEMP_PATH_H
#define KSYNC_TEMP_PATH_H

#include <filesystem>

#include "../../utilities/utilities.h"

namespace kysync {

class TempPath final {
  const std::filesystem::path path_;
  const bool keep_;

public:
  TempPath();

  TempPath(const std::filesystem::path &parent_path, bool keep);

  ~TempPath();

  std::filesystem::path GetPath() const;
};

}  // namespace kysync

#endif  // KSYNC_TEMP_PATH_H
