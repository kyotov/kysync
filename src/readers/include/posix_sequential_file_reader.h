#ifndef KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_POSIX_SEQUENTIAL_FILE_READER_H
#define KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_POSIX_SEQUENTIAL_FILE_READER_H

#include <kysync/readers/reader.h>

#include <filesystem>
#include <fstream>

namespace kysync {

class PosixSequentialFileReader final : public Reader {
  std::filesystem::path path_;

  int fd_;
  std::streamoff cached_current_offset_;

public:
  explicit PosixSequentialFileReader(const std::filesystem::path &path);
  virtual ~PosixSequentialFileReader();

  [[nodiscard]] std::streamsize GetSize() const override;

  std::streamsize
  Read(void *buffer, std::streamoff offset, std::streamsize size) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_POSIX_SEQUENTIAL_FILE_READER_H
