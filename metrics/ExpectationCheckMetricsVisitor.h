#ifndef KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H
#define KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H

#include <gtest/gtest.h>

#include <map>

#include "MetricVisitor.h"

class ExpectationCheckMetricVisitor final : public MetricVisitor {
public:
  explicit ExpectationCheckMetricVisitor(
      MetricContainer &host,
      std::map<std::string, uint64_t> &&_expectations);

  ~ExpectationCheckMetricVisitor();

  void Visit(const std::string &name, const Metric &value) override;

  void Visit(const std::string &name, const MetricContainer &container)
      override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H
