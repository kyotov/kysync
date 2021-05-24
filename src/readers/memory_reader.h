#ifndef KSYNC_MEMORY_READER_H
#define KSYNC_MEMORY_READER_H

#include <memory>

#include "reader.h"

namespace kysync {

class MemoryReader final : public Reader {
  const void* const kBuffer;
  const size_t kSize;

public:
  MemoryReader(const void *buffer, size_t size);

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) override;
};

}  // namespace kysync

#endif  // KSYNC_MEMORY_READER_H
