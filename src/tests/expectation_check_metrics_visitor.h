#ifndef KSYNC_EXPECTATION_CHECK_METRICS_VISITOR_H
#define KSYNC_EXPECTATION_CHECK_METRICS_VISITOR_H

#include <gtest/gtest.h>

#include <map>

#include "../metrics/metric_visitor.h"
#include "../utilities/utilities.h"

namespace kysync {

class ExpectationCheckMetricVisitor final : public MetricVisitor {
  PIMPL;
  NO_COPY_OR_MOVE(ExpectationCheckMetricVisitor);

public:
  explicit ExpectationCheckMetricVisitor(
      MetricContainer &host,
      std::map<std::string, uint64_t> &&expectations);

  ~ExpectationCheckMetricVisitor();

  void Visit(const std::string &name, Metric &value) override;

  void Visit(const std::string &name, MetricContainer &container) override;
};

}  // namespace kysync

#endif  // KSYNC_EXPECTATION_CHECK_METRICS_VISITOR_H
