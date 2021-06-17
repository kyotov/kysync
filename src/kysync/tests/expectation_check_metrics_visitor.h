#ifndef KSYNC_SRC_TESTS_EXPECTATION_CHECK_METRICS_VISITOR_H
#define KSYNC_SRC_TESTS_EXPECTATION_CHECK_METRICS_VISITOR_H

#include <gtest/gtest.h>
#include <ky/metrics/metrics.h>

#include <map>

namespace kysync {

class ExpectationCheckMetricVisitor final : public ky::metrics::MetricVisitor {
  std::map<std::string, uint64_t> expectations_;
  std::map<std::string, uint64_t> unchecked_;
  std::string context_;

public:
  explicit ExpectationCheckMetricVisitor(
      ky::metrics::MetricContainer &host,
      std::map<std::string, uint64_t> &&expectations);

  ~ExpectationCheckMetricVisitor();

  void Visit(const std::string &name, ky::metrics::Metric &value) override;

  void Visit(const std::string &name, ky::metrics::MetricContainer &container)
      override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_EXPECTATION_CHECK_METRICS_VISITOR_H
