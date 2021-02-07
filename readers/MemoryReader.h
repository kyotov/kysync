#ifndef KSYNC_MEMORYREADER_H
#define KSYNC_MEMORYREADER_H

#include <memory>

#include "Reader.h"

class MemoryReader final : public Reader {
public:
  MemoryReader(const void *buffer, size_t size);

  ~MemoryReader() override;

  [[nodiscard]] size_t size() const override;

  size_t read(void *buffer, size_t offset, size_t size) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_MEMORYREADER_H
