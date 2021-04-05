#ifndef KSYNC_TEMPPATH_H
#define KSYNC_TEMPPATH_H

#include <filesystem>

#include "../utilities/utilities.h"

namespace kysync {

class TempPath {
  PIMPL;
  NO_COPY_OR_MOVE(TempPath);

public:
  TempPath(bool keep = false);

  ~TempPath();

  [[nodiscard]] std::filesystem::path GetPath() const;
};

}  // namespace kysync

#endif  // KSYNC_TEMPPATH_H
