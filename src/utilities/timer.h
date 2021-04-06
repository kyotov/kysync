#ifndef KSYNC2_TIMER_H
#define KSYNC2_TIMER_H

#include <chrono>

namespace kysync {

using Timestamp = decltype(std::chrono::high_resolution_clock::now());
using Milliseconds = std::chrono::milliseconds;

Timestamp Now();

size_t DeltaMs(Timestamp beg, Timestamp end);

}  // namespace kysync

#endif  // KSYNC2_TIMER_H
