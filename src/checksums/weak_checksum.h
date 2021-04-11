#ifndef KSYNC_WEAK_CHECKSUM_H
#define KSYNC_WEAK_CHECKSUM_H

#include <cstdint>
#include <functional>

namespace kysync {

/**
 * computes a simple buffer checksum ala rsync
 *
 * @param buffer
 * @param size
 * @return
 */
uint32_t WeakChecksum(const void *buffer, size_t size);

/**
 * call back function type used by rolling window checksum
 * this is called after each byte of data is processed
 * parameters passed are:
 * - the window offset from the original buffer (always negative!)
 * - the checksum of the data in the window
 * NOTE: the window size is assumed to be known to the user...
 */
using WeakChecksumCallback = std::function<void(size_t offset, uint32_t wcs)>;

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
 * @param buffer
 * @param size
 * @param running_checksum
 * @param callback
 * @return
 */
uint32_t WeakChecksum(
    const void *buffer,
    size_t size,
    uint32_t running_checksum,
    const WeakChecksumCallback &callback);

}  // namespace kysync

#endif  // KSYNC_WEAK_CHECKSUM_H
