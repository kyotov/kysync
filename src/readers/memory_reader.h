#ifndef KSYNC_MEMORY_READER_H
#define KSYNC_MEMORY_READER_H

#include <memory>

#include "reader.h"

namespace kysync {

class MemoryReader final : public Reader {
public:
  MemoryReader(const void *data, size_t data_size);

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) override;

  const void* const data;
  const size_t data_size;
};

}  // namespace kysync

#endif  // KSYNC_MEMORY_READER_H
