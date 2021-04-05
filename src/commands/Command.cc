#include "Command.h"

#include "../metrics/Metric.h"
#include "../metrics/MetricVisitor.h"
#include "impl/CommandImpl.h"

namespace kysync {

void Command::Impl::Accept(MetricVisitor &visitor) {
  VISIT_METRICS(progress_monitor_enabled_);
  VISIT_METRICS(progress_phase_);
  VISIT_METRICS(progress_next_phase_);
  VISIT_METRICS(progress_total_bytes_);
  VISIT_METRICS(progress_current_bytes_);
}

Command::Command() : impl_(std::make_unique<Impl>()) {}

Command::Command(Command &&) noexcept = default;

Command::~Command() = default;

void Command::Accept(MetricVisitor &visitor) { return impl_->Accept(visitor); }

void Command::StartNextPhase(MetricValueType size) const {
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
