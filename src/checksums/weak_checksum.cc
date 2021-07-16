#include <kysync/checksums/weak_checksum.h>

// TODO(kyotov): add a link to the rsync paper / algorithm

namespace kysync {

uint32_t WeakChecksum(const void *buffer, std::streamsize size) {
  const auto *data = static_cast<const char *>(buffer);

  uint16_t a = 0;
  uint16_t b = 0;

  for (std::streamsize i = 0; i < size; i++) {
    a += data[i];
    b += (size - i) * data[i];
  }

  return b << 16 | a;
}

}  // namespace kysync