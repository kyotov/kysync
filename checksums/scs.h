#ifndef KSYNC_SCS_H
#define KSYNC_SCS_H

#include <cstdint>
#include <istream>
#include <string>

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
  const uint64_t hi = 0;
  const uint64_t lo = 0;

  static StrongChecksum compute(const void *buffer, size_t size);

  static StrongChecksum compute(std::istream &input);

  bool operator==(const StrongChecksum& other) const;

  [[nodiscard]] std::string toString() const;
};

#endif  // KSYNC_SCS_H
