#ifndef KSYNC_METRICCONTAINER_H
#define KSYNC_METRICCONTAINER_H

class MetricVisitor;

class MetricContainer {
public:
  virtual void accept(MetricVisitor &visitor) const = 0;
};

#endif  // KSYNC_METRICCONTAINER_H
