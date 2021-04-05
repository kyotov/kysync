#ifndef KSYNC_STRONGCHECKSUM_H
#define KSYNC_STRONGCHECKSUM_H

#include <cstdint>
#include <istream>
#include <string>

namespace kysync {

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
  const uint64_t kHi = 0;
  const uint64_t kLo = 0;

  static StrongChecksum Compute(const void *buffer, size_t size);

  static StrongChecksum Compute(std::istream &input);

  bool operator==(const StrongChecksum &other) const;

  [[nodiscard]] std::string ToString() const;
};

}  // namespace kysync

#endif  // KSYNC_STRONGCHECKSUM_H
