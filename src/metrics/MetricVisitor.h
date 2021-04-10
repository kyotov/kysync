#ifndef KSYNC_METRICVISITOR_H
#define KSYNC_METRICVISITOR_H

#include <string>

#include "Metric.h"
#include "MetricContainer.h"

#define VISIT_METRICS(host) visitor.Visit(std::string(#host), host)

namespace kysync {

class MetricVisitor {
public:
  virtual void Visit(const std::string &name, Metric &value) = 0;

  virtual void Visit(const std::string &name, MetricContainer &container) = 0;
};

}  // namespace kysync

#endif  // KSYNC_METRICVISITOR_H
