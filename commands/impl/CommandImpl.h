#ifndef KSYNC_COMMANDIMPL_H
#define KSYNC_COMMANDIMPL_H

struct Command::Impl final {
  Metric progressPhase;
  Metric progressTotalBytes;
  Metric progressCurrentBytes;

  void accept(MetricVisitor &visitor) const;
};

#endif  // KSYNC_COMMANDIMPL_H
