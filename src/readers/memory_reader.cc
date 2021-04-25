#include "memory_reader.h"

#include <cstring>

namespace kysync {

class MemoryReader::Impl {
public:
  const void* const kBuffer;
  const size_t kSize;

  Impl(const void* buffer, size_t size) : kBuffer(buffer), kSize(size) {}
};

MemoryReader::MemoryReader(const void* buffer, size_t size)
    : impl_(std::make_unique<Impl>(buffer, size)) {}

MemoryReader::~MemoryReader() = default;

size_t MemoryReader::GetSize() const { return impl_->kSize; }

size_t MemoryReader::Read(void* buffer, size_t offset, size_t size) const {
  auto limit = std::min(impl_->kSize, offset + size);
  auto count = offset < limit ? limit - offset : 0;

  memcpy(buffer, (uint8_t*)impl_->kBuffer + offset, count);

  // make sure the metrics are captured!
  return Reader::Read(buffer, offset, count);
}

}  // namespace kysync