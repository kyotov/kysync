#include "memory_reader.h"

#include <cstring>

namespace kysync {

MemoryReader::MemoryReader(const void* buffer, size_t size)
    : kBuffer(buffer),
      kSize(size) {}

size_t MemoryReader::GetSize() const { return kSize; }

size_t MemoryReader::Read(void* buffer, size_t offset, size_t size) {
  auto limit = std::min(kSize, offset + size);
  auto count = offset < limit ? limit - offset : 0;

  memcpy(buffer, (uint8_t*)kBuffer + offset, count);

  // make sure the metrics are captured!
  return Reader::Read(buffer, offset, count);
}

}  // namespace kysync