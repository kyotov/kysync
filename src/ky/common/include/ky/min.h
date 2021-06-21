#ifndef KSYNC_SRC_CONFIG_H
#define KSYNC_SRC_CONFIG_H

#include <cstdint>

namespace ky {

// std::streamsize and std::streamoff are different on MacOS / xcode 12
// one is long, the other is long long
// this throws off std::min, so we use our own
// sad...
constexpr intmax_t Min(intmax_t x, intmax_t y) { return x < y ? x : y; }

}  // namespace ky

#endif  // KSYNC_SRC_CONFIG_H
