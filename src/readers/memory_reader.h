#ifndef KSYNC_MEMORY_READER_H
#define KSYNC_MEMORY_READER_H

#include <memory>

#include "reader.h"

namespace kysync {

class MemoryReader final : public Reader {
  const void *data_;
  std::streamsize data_size_;

public:
  MemoryReader(const void *data, std::streamsize data_size);

  [[nodiscard]] std::streamsize GetSize() const override;

  std::streamsize Read(void *buffer, std::streamoff offset, std::streamsize size)
      override;
};

}  // namespace kysync

#endif  // KSYNC_MEMORY_READER_H
