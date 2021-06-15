#ifndef KSYNC_SRC_UTILITIES_FILESTREAM_H
#define KSYNC_SRC_UTILITIES_FILESTREAM_H

#include <filesystem>

namespace kysync {

class FileStream {
  std::filesystem::path path_;

public:
  explicit FileStream(std::filesystem::path path);

  void Resize(std::streamsize size) const;

  std::fstream GetStream() const;
};

}  // namespace kysync

#endif  // KSYNC_SRC_UTILITIES_FILESTREAM_H
