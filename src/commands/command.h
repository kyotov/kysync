#ifndef KSYNC__SRC_COMMANDS_COMMAND_H
#define KSYNC__SRC_COMMANDS_COMMAND_H

#include <memory>

#include "../metrics/metric.h"
#include "../metrics/metric_container.h"

namespace kysync {

class Command : public MetricContainer {
  Metric progress_monitor_enabled_;
  Metric progress_phase_;
  Metric progress_next_phase_;
  Metric progress_total_bytes_;
  Metric progress_current_bytes_;

public:
  virtual int Run() = 0;

  void Accept(MetricVisitor &visitor) override;

  void StartNextPhase(MetricValueType size);

  MetricValueType AdvanceProgress(MetricValueType amount);
};

}  // namespace kysync

#endif  // KSYNC__SRC_COMMANDS_COMMAND_H
