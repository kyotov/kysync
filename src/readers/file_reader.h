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

  [[nodiscard]] std::streamsize GetSize() const override;

  std::streamsize
  Read(void *buffer, std::streamoff offset, std::streamsize size) override;
};

}  // namespace kysync

#endif  // KSYNC_FILE_READER_H
