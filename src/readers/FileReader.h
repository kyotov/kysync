#ifndef KSYNC_FILEREADER_H
#define KSYNC_FILEREADER_H

#include <filesystem>

#include "Reader.h"

namespace kysync {

class FileReader final : public Reader {
  PIMPL;
  NO_COPY_OR_MOVE(FileReader);

public:
  explicit FileReader(const std::filesystem::path &path);

  ~FileReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;
};

}  // namespace kysync

#endif  // KSYNC_FILEREADER_H
