#include "timer.h"

namespace kysync {

Timestamp Now() { return std::chrono::high_resolution_clock::now(); }

intmax_t DeltaMs(Timestamp beg, Timestamp end) {
  return std::chrono::duration_cast<Milliseconds>(end - beg).count();
}

}  // namespace kysync
