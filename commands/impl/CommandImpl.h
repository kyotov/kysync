#ifndef KSYNC_COMMANDIMPL_H
#define KSYNC_COMMANDIMPL_H

struct Command::Impl final {
  Metric progress_phase_;
  Metric progress_total_bytes_;
  Metric progress_current_bytes_;
  Metric progress_compressed_bytes_;

  void Accept(MetricVisitor &visitor) const;
};

#endif  // KSYNC_COMMANDIMPL_H
