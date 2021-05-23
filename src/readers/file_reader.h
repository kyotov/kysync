#ifndef KSYNC_FILE_READER_H
#define KSYNC_FILE_READER_H

#include <filesystem>
#include <fstream>

#include "reader.h"

namespace kysync {

class FileReader final : public Reader {
  std::filesystem::path path_;
  std::ifstream data_;

public:
  explicit FileReader(const std::filesystem::path &path);

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) override;
};

}  // namespace kysync

#endif  // KSYNC_FILE_READER_H
