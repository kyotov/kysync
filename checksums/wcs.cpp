#include "wcs.h"

// TODO: add a link to the rsync paper / algorithm

uint32_t weakChecksum(const void *buffer, size_t size)
{
  auto data = (uint8_t *)buffer;

  uint16_t a = 0;
  uint16_t b = 0;

  for (size_t i = 0; i < size; i++) {
    a += data[i];
    b += (size - i) * data[i];
  }

  return b << 16 | a;
}

uint32_t weakChecksum(
    const void *buffer,
    size_t size,
    uint32_t runningChecksum,
    const WeakChecksumCallback &callback)
{
  auto *data = (uint8_t *)buffer;

  uint16_t a = runningChecksum & 0xFFFF;
  uint16_t b = runningChecksum >> 16;

  for (size_t i = 0; i < size; i++) {
    a += data[i] - data[i - size];
    b += a - size * data[i - size];
    callback(i + 1 - size, b << 16 | a);
  }

  return b << 16 | a;
}