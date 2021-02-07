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

  void visit(const std::string &name, Metric &value) override;

  void visit(const std::string &name, MetricContainer &container) override;

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H
