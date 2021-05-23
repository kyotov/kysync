#ifndef KSYNC_MEMORY_READER_H
#define KSYNC_MEMORY_READER_H

#include <memory>

#include "reader.h"

namespace kysync {

class MemoryReader final : public Reader {
  const void *data_;
  size_t data_size_;

public:
  MemoryReader(const void *data, size_t data_size);

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) override;
};

}  // namespace kysync

#endif  // KSYNC_MEMORY_READER_H
