#ifndef KSYNC_METRIC_H
#define KSYNC_METRIC_H

#include <atomic>
#include <cstdint>

namespace kysync {

using Metric = std::atomic<uint64_t>;

}  // namespace kysync

#endif  // KSYNC_METRIC_H
