#include "strong_checksum.h"

#include <xxhash.h>

#include <iomanip>
#include <sstream>

namespace kysync {

StrongChecksum StrongChecksum::Compute(const void *buffer, size_t size) {
  auto digest = XXH3_128bits(buffer, size);

  return {digest.high64, digest.low64};
}

StrongChecksum StrongChecksum::Compute(std::istream &input) {
  constexpr size_t kBufSize = 64 * 1024;

  auto *state = XXH3_createState();

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
  return kHi == other.kHi && kLo == other.kLo;
}

std::string StrongChecksum::ToString() const {
  std::stringstream s;
  s << std::hex << std::setw(16) << std::setfill('0') << kHi << kLo;
  return s.str();
}

}  // namespace kysync
