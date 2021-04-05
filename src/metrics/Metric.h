#ifndef KSYNC_METRIC_H
#define KSYNC_METRIC_H

#include <atomic>
#include <cstdint>

namespace kysync {

using MetricValueType = uint64_t;
using Metric = std::atomic<MetricValueType>;

}  // namespace kysync

#endif  // KSYNC_METRIC_H
