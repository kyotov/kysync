#ifndef KSYNC_FILEREADER_H
#define KSYNC_FILEREADER_H

#include <filesystem>

#include "Reader.h"

class FileReader : public Reader {
public:
  explicit FileReader(const std::filesystem::path &path);

  ~FileReader() override;

  [[nodiscard]] size_t size() const override;

  size_t read(void *buffer, size_t offset, size_t size) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_FILEREADER_H
