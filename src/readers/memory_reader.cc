#include "memory_reader.h"

#include <cstring>

namespace kysync {

MemoryReader::MemoryReader(const void* data, size_t data_size)
    : data_(data),
      data_size_(data_size) {}

size_t MemoryReader::GetSize() const { return data_size_; }

size_t MemoryReader::Read(void* buffer, size_t offset, size_t size) {
  auto limit = std::min(data_size_, offset + size);
  auto count = offset < limit ? limit - offset : 0;

  memcpy(buffer, (uint8_t*)data_ + offset, count);

  // make sure the metrics are captured!
  return Reader::Read(buffer, offset, count);
}

}  // namespace kysync