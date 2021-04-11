#ifndef KSYNC_METRIC_CONTAINER_H
#define KSYNC_METRIC_CONTAINER_H

namespace kysync {

class MetricVisitor;

class MetricContainer {
public:
  virtual void Accept(MetricVisitor &visitor) = 0;
};

}  // namespace kysync

#endif  // KSYNC_METRIC_CONTAINER_H
