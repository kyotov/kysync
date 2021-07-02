#include <ky/min.h>
#include <kysync/readers/memory_reader.h>

#include <cstring>

namespace kysync {

MemoryReader::MemoryReader(const void* data, std::streamsize data_size)
    : data_(data),
      data_size_(data_size) {}

std::streamsize MemoryReader::GetSize() const { return data_size_; }

std::streamsize
MemoryReader::Read(void* buffer, std::streamoff offset, std::streamsize size) {
  auto limit = ky::Min(data_size_, offset + size);
  auto count = offset < limit ? limit - offset : 0;

  memcpy(buffer, static_cast<const char*>(data_) + offset, count);

  // make sure the metrics are captured!
  return Reader::Read(buffer, offset, count);
}

}  // namespace kysync