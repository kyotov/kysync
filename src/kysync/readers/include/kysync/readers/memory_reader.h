#ifndef KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_MEMORY_READER_H
#define KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_MEMORY_READER_H

#include <kysync/readers/reader.h>

#include <memory>

namespace kysync {

class MemoryReader final : public Reader {
  const void *data_;
  std::streamsize data_size_;

public:
  MemoryReader(const void *data, std::streamsize data_size);

  [[nodiscard]] std::streamsize GetSize() const override;

  std::streamsize
  Read(void *buffer, std::streamoff offset, std::streamsize size) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_READERS_INCLUDE_KYSYNC_READERS_MEMORY_READER_H
