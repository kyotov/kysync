#ifndef KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_FILE_READER_H
#define KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_FILE_READER_H

#include <kysync/readers/reader.h>

#include <filesystem>
#include <fstream>

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

#endif  // KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_FILE_READER_H
