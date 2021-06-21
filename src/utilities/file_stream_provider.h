#ifndef KSYNC_SRC_UTILITIES_FILE_STREAM_PROVIDER_H
#define KSYNC_SRC_UTILITIES_FILE_STREAM_PROVIDER_H

#include <filesystem>

namespace kysync {

class FileStreamProvider {
  std::filesystem::path path_;

public:
  explicit FileStreamProvider(std::filesystem::path path);

  void Resize(std::streamsize size) const;

  [[nodiscard]] std::fstream CreateFileStream() const;
};

}  // namespace kysync

#endif  // KSYNC_SRC_UTILITIES_FILE_STREAM_PROVIDER_H
