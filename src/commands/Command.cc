#include "Command.h"

#include "../metrics/Metric.h"
#include "../metrics/MetricVisitor.h"
#include "impl/CommandImpl.h"

namespace kysync {

void Command::Impl::Accept(MetricVisitor &visitor) const {
  VISIT_METRICS(progress_phase_);
  VISIT_METRICS(progress_total_bytes_);
  VISIT_METRICS(progress_current_bytes_);
  VISIT_METRICS(progress_compressed_bytes_);
}

Command::Command() : impl_(std::make_unique<Impl>()) {}

Command::Command(Command &&) noexcept = default;

Command::~Command() = default;

void Command::Accept(MetricVisitor &visitor) const {
  return impl_->Accept(visitor);
}

}  // namespace kysync