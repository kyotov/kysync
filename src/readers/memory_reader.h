#ifndef KSYNC_MEMORY_READER_H
#define KSYNC_MEMORY_READER_H

#include <memory>

#include "reader.h"

namespace kysync {

class MemoryReader final : public Reader {
  PIMPL;
  NO_COPY_OR_MOVE(MemoryReader);

public:
  MemoryReader(const void *buffer, size_t size);

  ~MemoryReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;
};

}  // namespace kysync

#endif  // KSYNC_MEMORY_READER_H
