#ifndef KSYNC_FILE_READER_H
#define KSYNC_FILE_READER_H

#include <filesystem>

#include "reader.h"

namespace kysync {

class FileReader final : public Reader {
  PIMPL;
  NO_COPY_OR_MOVE(FileReader);

public:
  explicit FileReader(const std::filesystem::path &path);

  ~FileReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;
  size_t Read(
    void *buffer,
    std::vector<BatchedRetrivalInfo> &batched_retrievals_info) const override;
};

}  // namespace kysync

#endif  // KSYNC_FILE_READER_H
