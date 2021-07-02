#ifndef KSYNC_SRC_KY_TIMER_TIMER_H
#define KSYNC_SRC_KY_TIMER_TIMER_H

#include <chrono>

namespace ky::timer {

using Timestamp = decltype(std::chrono::high_resolution_clock::now());
using Milliseconds = std::chrono::milliseconds;

Timestamp Now();

intmax_t DeltaMs(Timestamp beg, Timestamp end);

}  // namespace ky::timer

#endif  // KSYNC_SRC_KY_TIMER_TIMER_H
