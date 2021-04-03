#include "wcs.h"

// TODO: add a link to the rsync paper / algorithm

uint32_t WeakChecksum(const void *buffer, size_t size) {
  auto data = (uint8_t *)buffer;

  uint16_t a = 0;
  uint16_t b = 0;

  for (size_t i = 0; i < size; i++) {
    a += data[i];
    b += (size - i) * data[i];
  }

  return b << 16 | a;
}

uint32_t WeakChecksum(
    const void *buffer,
    size_t size,
    uint32_t running_checksum,
    const WeakChecksumCallback &callback) {
  auto *data = (uint8_t *)buffer;

  auto a = static_cast<uint16_t>(running_checksum & 0xFFFF);
  auto b = static_cast<uint16_t>(running_checksum >> 16);

  for (size_t i = 0; i < size; i++) {
    a += data[i] - data[i - size];
    b += a - size * data[i - size];
    callback(i + 1 - size, b << 16 | a);
  }

  return b << 16 | a;
}