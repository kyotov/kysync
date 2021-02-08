#ifndef KSYNC_METRICVISITOR_H
#define KSYNC_METRICVISITOR_H

#include <string>

#include "Metric.h"
#include "MetricContainer.h"

#define VISIT(visitor, host) visitor.visit(std::string(#host), host)

class MetricVisitor {
public:
  virtual void visit(const std::string &name, const Metric &value) = 0;

  virtual void visit(
      const std::string &name,
      const MetricContainer &container) = 0;
};

#endif  // KSYNC_METRICVISITOR_H
