#include "streams.h"

#include <glog/logging.h>

#include <random>

#include "../checksums/strong_checksum.h"

namespace kysync {

std::streamsize
StreamWrite(std::ostream &stream, const void *data, std::streamsize size) {
  CHECK_GE(size, 0);
  stream.write(static_cast<const char *>(data), size);
  CHECK(stream);
  return size;
}

}  // namespace kysync
