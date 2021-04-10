#include "command.h"

#include "../metrics/metric.h"
#include "../metrics/metric_visitor.h"
#include "impl/command_impl.h"

namespace kysync {

void Command::Impl::Accept(MetricVisitor &visitor) {
  VISIT_METRICS(progress_monitor_enabled_);
  VISIT_METRICS(progress_phase_);
  VISIT_METRICS(progress_next_phase_);
  VISIT_METRICS(progress_total_bytes_);
  VISIT_METRICS(progress_current_bytes_);
}

Command::Command() : impl_(std::make_unique<Impl>()) {}

Command::~Command() = default;

void Command::Accept(MetricVisitor &visitor) { return impl_->Accept(visitor); }

void Command::StartNextPhase(MetricValueType size) const {
  /*
   * Requests moving to the next phase of a command.
   * This can be done unilaterally by the command, *or* if progress monitoring
   * is enabled, it can be controlled from outside.
   *
   * Progress monitoring is enabled, when the Command's Run method is executed
   * by a Monitor. In such context the command merely requests to move to the
   * next phase by incrementing the progress_next_phase_ metric. The monitor
   * then knows to collect statistics of the current phase and allow the command
   * to progress to the next phase (by incrementing progress_phase_). These
   * statistics collected by the monitor can be used for better UX on the
   * command line as well as for studying performance aspects of the
   * implementation.
   *
   * In the absence of a Monitor, the command increments progress_phase_ itself.
   */
  impl_->progress_next_phase_++;
  if (impl_->progress_monitor_enabled_ != 0) {
    while (impl_->progress_next_phase_ != impl_->progress_phase_.load()) {
    }
  } else {
    impl_->progress_phase_ = impl_->progress_next_phase_.load();
  }
  impl_->progress_total_bytes_ = size;
  impl_->progress_current_bytes_ = 0;
}

MetricValueType Command::AdvanceProgress(MetricValueType amount) const {
  impl_->progress_current_bytes_ += amount;
  impl_->progress_total_bytes_.store(
      std::max(impl_->progress_total_bytes_, impl_->progress_current_bytes_));
  return impl_->progress_current_bytes_;
}

}  // namespace kysync
