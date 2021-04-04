#ifndef KSYNC_MEMORYREADER_H
#define KSYNC_MEMORYREADER_H

#include <memory>

#include "Reader.h"

namespace kysync {

class MemoryReader final : public Reader {
public:
  MemoryReader(const void *buffer, size_t size);

  ~MemoryReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kysync

#endif  // KSYNC_MEMORYREADER_H
