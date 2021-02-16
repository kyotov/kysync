#include "StrongChecksum.h"

#include <xxhash.h>

#include <iomanip>
#include <sstream>

StrongChecksum StrongChecksum::compute(const void *buffer, size_t size)
{
  auto digest = XXH3_128bits(buffer, size);

  return {digest.high64, digest.low64};
}

StrongChecksum StrongChecksum::compute(std::istream &input)
{
  constexpr size_t BUF_SIZE = 64 * 1024;

  auto state = XXH3_createState();

  char buffer[BUF_SIZE];

  XXH3_128bits_reset(state);
  while (input) {
    input.read(buffer, BUF_SIZE);
    XXH3_128bits_update(state, buffer, input.gcount());
  }
  auto digest = XXH3_128bits_digest(state);

  return {digest.high64, digest.low64};
}

bool StrongChecksum::operator==(const StrongChecksum &other) const
{
  return hi == other.hi && lo == other.lo;
}

std::string StrongChecksum::toString() const
{
  std::stringstream s;
  s << std::hex << std::setw(16) << std::setfill('0') << hi << lo;
  return s.str();
}
