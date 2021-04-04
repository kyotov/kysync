#ifndef KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H
#define KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H

#include <gtest/gtest.h>

#include <map>

#include "MetricVisitor.h"

namespace kysync {

class ExpectationCheckMetricVisitor final : public MetricVisitor {
public:
  explicit ExpectationCheckMetricVisitor(
      MetricContainer &host,
      std::map<std::string, uint64_t> &&expectations);

  ~ExpectationCheckMetricVisitor();

  void Visit(const std::string &name, const Metric &value) override;

  void Visit(const std::string &name, const MetricContainer &container)
      override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kysync

#endif  // KSYNC_EXPECTATIONCHECKMETRICSVISITOR_H
