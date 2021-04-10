#include "streams.h"

#include <glog/logging.h>

#include "../checksums/StrongChecksum.h"

namespace kysync {

size_t StreamWrite(std::ostream &stream, const void *data, size_t size) {
  stream.write(static_cast<const char *>(data), size);
  CHECK(stream);
  return size;
}

template <typename T>
size_t StreamWrite(
    std::ostream &stream,
    std::vector<T> data,
    size_t max_size_to_write) {
  auto size_to_write = std::min(data.size() * sizeof(T), max_size_to_write);

  return StreamWrite(
      stream,
      data.data(),
      std::max(static_cast<size_t>(0), size_to_write));
}

// StreamWrite specializations

template size_t
StreamWrite<uint32_t>(std::ostream &, std::vector<uint32_t>, size_t);

template size_t
StreamWrite<uint64_t>(std::ostream &, std::vector<uint64_t>, size_t);

template size_t StreamWrite<StrongChecksum>(
    std::ostream &,
    std::vector<StrongChecksum>,
    size_t);

}  // namespace kysync