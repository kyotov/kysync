#ifndef KSYNC_WEAK_CHECKSUM_H
#define KSYNC_WEAK_CHECKSUM_H

#include <cstdint>
#include <functional>
#include <ios>

namespace kysync {

/**
 * computes a simple buffer checksum ala rsync
 *
 * @param buffer
 * @param size
 * @return
 */
uint32_t WeakChecksum(const void *buffer, std::streamsize size);

/**
 * computes a running window checksum ala rsync
 *
 * - the window size is the same as the buffer size
 *   this keeps things simple but we may relax this in the future...
 *
 * - call this function repeatedly on successive blocks of data
 * - pass the result of the previous call to the runningChecksum parameter
 * - use runningChecksum==0 for the first block of a sequence
 *
 * - the callback is called after each byte is processed
 *
 * - NOTE: the function accesses buffer[-size + 1]..buffer[size - 1]
 *   even if it is the first block!
 *   so the first block should have a sentinel block filled with 0s before it.
 *
 * callback param is a functor used by rolling window checksum
 * this is called after each byte of data is processed
 * parameters passed are:
 * - the window offset from the original buffer (always negative!)
 * - the checksum of the data in the window
 *
 * NOTE: the window size is assumed to be known to the user...
 *
 * @param buffer
 * @param size
 * @param running_checksum
 * @param callback
 * @return
 */
template <typename T>
uint32_t WeakChecksum(
    const void *buffer,
    std::streamsize size,
    uint32_t running_checksum,
    const T &callback) {
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
#endif  // KSYNC_WEAK_CHECKSUM_H
