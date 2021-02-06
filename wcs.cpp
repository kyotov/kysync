#include "wcs.h"

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
    const WeakChecksumCallback &callback,
    bool warmup)
{
  auto *data = (uint8_t *)buffer;

  uint16_t a = runningChecksum & 0xFFFF;
  uint16_t b = runningChecksum >> 16;

  for (size_t i = 0; i < size; i++) {
    // update the window
    a += data[i] - data[i - size];
    b += a - size * data[i - size];

    // callback with the result for the current window (unless in warmup)
    if (!warmup) {
      callback(data + i + 1 - size, size, b << 16 | a);
    }
  }

  return b << 16 | a;
}