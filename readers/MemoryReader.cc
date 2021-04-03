#include "MemoryReader.h"

#include <cstring>

struct MemoryReader::Impl {
  const void* buffer;
  const size_t size;

  Impl(const void* _buffer, size_t _size) : buffer(_buffer), size(_size) {}
};

MemoryReader::MemoryReader(const void* buffer, size_t size)
    : impl_(std::make_unique<Impl>(buffer, size)) {}

MemoryReader::~MemoryReader() = default;

size_t MemoryReader::GetSize() const { return impl_->size; }

size_t MemoryReader::Read(void* buffer, size_t offset, size_t size) const {
  auto limit = std::min(impl_->size, offset + size);
  auto count = offset < limit ? limit - offset : 0;

  memcpy(buffer, (uint8_t*)impl_->buffer + offset, count);

  // make sure the metrics are captured!
  return Reader::Read(buffer, offset, count);
}
