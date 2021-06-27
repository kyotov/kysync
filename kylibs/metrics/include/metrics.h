#ifndef KSYNC_SRC_KY_METRICS_METRICS_H
#define KSYNC_SRC_KY_METRICS_METRICS_H

#include <atomic>
#include <cstdint>
#include <string>

// NOTE: using #host -- impossible without macro
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define VISIT_METRICS(host) visitor.Visit(std::string(#host), host)

namespace ky::metrics {

using MetricValueType = intmax_t;

using Metric = std::atomic<MetricValueType>;

class MetricVisitor;

/***
 * Metric Container
 */
class MetricContainer {
public:
  virtual void Accept(MetricVisitor &visitor) = 0;
};

/***
 * Metric Visitor
 */
class MetricVisitor {
public:
  virtual void Visit(const std::string &name, Metric &value) = 0;

  virtual void Visit(const std::string &name, MetricContainer &container) = 0;
};

}  // namespace ky::metrics

#endif  // KSYNC_SRC_KY_METRICS_METRICS_H
