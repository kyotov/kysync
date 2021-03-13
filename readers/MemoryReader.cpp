#include "MemoryReader.h"
#include <cstring>

struct MemoryReader::Impl {
  const void* buffer;
  const size_t size;

  Impl(const void* _buffer, size_t _size)
      : buffer(_buffer),
        size(_size)
  {
  }
};

MemoryReader::MemoryReader(const void* buffer, size_t size)
    : pImpl(std::make_unique<Impl>(buffer, size))
{
}

MemoryReader::~MemoryReader() = default;

size_t MemoryReader::size() const
{
  return pImpl->size;
}

size_t MemoryReader::read(void* buffer, size_t offset, size_t size) const
{
  auto limit = std::min(pImpl->size, offset + size);
  auto count = offset < limit ? limit - offset : 0;

  memcpy(buffer, (uint8_t *)pImpl->buffer + offset, count);

  // make sure the metrics are captured!
  return Reader::read(buffer, offset, count);
}
