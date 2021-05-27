#ifndef KSYNC_STRONG_CHECKSUM_H
#define KSYNC_STRONG_CHECKSUM_H

#include <cstdint>
#include <istream>
#include <string>

namespace kysync {

/**
 * A class for computing 128bit strong checksums.
 * Current implementation uses https://github.com/Cyan4973/xxHash
 */
class StrongChecksum {
  uint64_t hi_;
  uint64_t lo_;

public:
  StrongChecksum();

  StrongChecksum(uint64_t hi, uint64_t lo);

  static StrongChecksum Compute(const void *buffer, std::streamsize size);

  static StrongChecksum Compute(std::istream &input);

  bool operator==(const StrongChecksum &other) const;

  [[nodiscard]] std::string ToString() const;
};

}  // namespace kysync

#endif  // KSYNC_STRONG_CHECKSUM_H
