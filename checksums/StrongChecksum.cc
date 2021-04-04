#include "StrongChecksum.h"

#include <xxhash.h>

#include <iomanip>
#include <sstream>

StrongChecksum StrongChecksum::Compute(const void *buffer, size_t size) {
  auto digest = XXH3_128bits(buffer, size);

  return {digest.high64, digest.low64};
}

StrongChecksum StrongChecksum::Compute(std::istream &input) {
  constexpr size_t kBufSize = 64 * 1024;

  auto state = XXH3_createState();

  char buffer[kBufSize];

  XXH3_128bits_reset(state);
  while (input) {
    input.read(buffer, kBufSize);
    XXH3_128bits_update(state, buffer, input.gcount());
  }
  auto digest = XXH3_128bits_digest(state);

  return {digest.high64, digest.low64};
}

bool StrongChecksum::operator==(const StrongChecksum &other) const {
  return hi_ == other.hi_ && lo_ == other.lo_;
}

std::string StrongChecksum::ToString() const {
  std::stringstream s;
  s << std::hex << std::setw(16) << std::setfill('0') << hi_ << lo_;
  return s.str();
}
