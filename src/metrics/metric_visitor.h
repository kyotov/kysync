#ifndef KSYNC_METRIC_VISITOR_H
#define KSYNC_METRIC_VISITOR_H

#include <string>

#include "metric.h"
#include "metric_container.h"

#define VISIT_METRICS(host) visitor.Visit(std::string(#host), host)

namespace kysync {

class MetricVisitor {
public:
  virtual void Visit(const std::string &name, Metric &value) = 0;

  virtual void Visit(const std::string &name, MetricContainer &container) = 0;
};

}  // namespace kysync

#endif  // KSYNC_METRIC_VISITOR_H
