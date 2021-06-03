#include "weak_checksum.h"

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

uint32_t WeakChecksum(
    const void *buffer,
    std::streamsize size,
    uint32_t running_checksum,
    const WeakChecksumCallback &callback) {
  const auto *data = static_cast<const char *>(buffer);

  auto a = static_cast<uint16_t>(running_checksum & 0xFFFF);
  auto b = static_cast<uint16_t>(running_checksum >> 16);

  for (std::streamsize i = 0; i < size; i++) {
    a += data[i] - data[i - size];
    b += a - size * data[i - size];
    callback(i + 1 - size, b << 16 | a);
  }

  return b << 16 | a;
}

}  // namespace kysync