#ifndef KSYNC_SRC_KY_METRICS_INCLUDE_KY_METRICS_METRIC_CALLBACK_VISITOR_H
#define KSYNC_SRC_KY_METRICS_INCLUDE_KY_METRICS_METRIC_CALLBACK_VISITOR_H

#include <ky/metrics/metrics.h>

#include <functional>
#include <string>
#include <vector>

namespace ky::metrics {

class MetricCallbackVisitor final : private MetricVisitor {
public:
  using Callback = std::function<void(const std::string &key, Metric &value)>;

  void Snapshot(Callback callback, MetricContainer &container);

private:
  Callback callback_;
  std::vector<std::string> context_;

  void Visit(const std::string &name, MetricContainer &container) override;
  void Visit(const std::string &name, Metric &value) override;
};

}  // namespace ky::metrics

#endif  // KSYNC_SRC_KY_METRICS_INCLUDE_KY_METRICS_METRIC_CALLBACK_VISITOR_H
