#ifndef KSYNC_STRONGCHECKSUM_H
#define KSYNC_STRONGCHECKSUM_H

#include <cstdint>
#include <istream>
#include <string>

// FIXME: rename header to StrongChecksum.h

/**
 * A class for computing 128bit strong checksums.
 * Current implementation uses https://github.com/Cyan4973/xxHash
 */
class StrongChecksum {
public:
  /* NOTE: I find this fascinating...
   * - you can create instances with initializer list, e.g. {2, 2}
   * - the initializers below are necessary for generating a default constructor
   */
  const uint64_t hi_ = 0;
  const uint64_t lo_ = 0;

  static StrongChecksum Compute(const void *buffer, size_t size);

  static StrongChecksum Compute(std::istream &input);

  bool operator==(const StrongChecksum &other) const;

  [[nodiscard]] std::string ToString() const;
};

#endif  // KSYNC_STRONGCHECKSUM_H
