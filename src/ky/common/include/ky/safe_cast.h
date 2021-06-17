#ifndef KSYNC_SRC_UTILITIES_SAFE_CAST_H
#define KSYNC_SRC_UTILITIES_SAFE_CAST_H

#include <glog/logging.h>

#include <limits>

namespace ky {

// template <std::signed_integral T, std::unsigned_integral F>
template <typename T, typename F>
inline T SafeCast(F value) {
  if (sizeof(T) <= sizeof(F)) {
    F max = std::numeric_limits<T>::max();
    LOG_ASSERT(value <= max) << " : " << value << " <= " << max;
  }
  return static_cast<T>(value);
}

}  // namespace ky

#endif  // KSYNC_SRC_UTILITIES_SAFE_CAST_H
