#ifndef KSYNC_FILE_READER_H
#define KSYNC_FILE_READER_H

#include <filesystem>
#include <fstream>

#include "reader.h"

namespace kysync {

class FileReader final : public Reader {
  std::ifstream data_;

public:
  const std::filesystem::path path;

  explicit FileReader(const std::filesystem::path &path);

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) override;
};

}  // namespace kysync

#endif  // KSYNC_FILE_READER_H
