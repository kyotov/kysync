#include <glog/logging.h>
#include <kysync/checksums/strong_checksum.h>
#include <kysync/streams.h>

#include <random>

namespace kysync {

std::streamsize
StreamWrite(std::ostream &stream, const void *data, std::streamsize size) {
  CHECK_GE(size, 0);
  stream.write(static_cast<const char *>(data), size);
  CHECK(stream);
  return size;
}

}  // namespace kysync
