#ifndef KSYNC_COMMANDIMPL_H
#define KSYNC_COMMANDIMPL_H

#include "../../metrics/Metric.h"
#include "../Command.h"

namespace kysync {

class Command::Impl final {
public:
  Metric progress_monitor_enabled_;
  Metric progress_phase_;
  Metric progress_next_phase_;
  Metric progress_total_bytes_;
  Metric progress_current_bytes_;

  void Accept(MetricVisitor &visitor);
};

}  // namespace kysync

#endif  // KSYNC_COMMANDIMPL_H
