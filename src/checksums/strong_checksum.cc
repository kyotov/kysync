#include "strong_checksum.h"

#include <xxhash.h>

#include <array>
#include <iomanip>
#include <sstream>

namespace kysync {

StrongChecksum::StrongChecksum() : StrongChecksum(0, 0) {}

StrongChecksum::StrongChecksum(uint64_t hi, uint64_t lo) : hi_(hi), lo_(lo) {}

StrongChecksum StrongChecksum::Compute(
    const void *buffer,
    std::streamsize size) {
  auto digest = XXH3_128bits(buffer, size);

  return {digest.high64, digest.low64};
}

StrongChecksum StrongChecksum::Compute(std::istream &input) {
  static constexpr std::streamsize kBufSize = 64 * 1024;

  auto *state = XXH3_createState();

  std::array<char, kBufSize> buffer{};

  XXH3_128bits_reset(state);
  while (input) {
    input.read(buffer.data(), kBufSize);
    XXH3_128bits_update(state, buffer.data(), input.gcount());
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

}  // namespace kysync
