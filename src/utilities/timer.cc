#include "timer.h"

namespace kysync {

Timestamp Now() {
  return std::chrono::high_resolution_clock::now();
}

size_t DeltaMs(Timestamp beg, Timestamp end) {
  return static_cast<size_t>(
      std::chrono::duration_cast<Milliseconds>(end - beg).count());
}

}  // namespace kysync
