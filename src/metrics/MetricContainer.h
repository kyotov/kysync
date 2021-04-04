#ifndef KSYNC_METRICCONTAINER_H
#define KSYNC_METRICCONTAINER_H

namespace kysync {

class MetricVisitor;

class MetricContainer {
public:
  virtual void Accept(MetricVisitor &visitor) const = 0;
};

}  // namespace kysync

#endif  // KSYNC_METRICCONTAINER_H
