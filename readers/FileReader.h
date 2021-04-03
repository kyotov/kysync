#ifndef KSYNC_FILEREADER_H
#define KSYNC_FILEREADER_H

#include <filesystem>

#include "Reader.h"

class FileReader : public Reader {
public:
  explicit FileReader(const std::filesystem::path &path);

  ~FileReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // KSYNC_FILEREADER_H
