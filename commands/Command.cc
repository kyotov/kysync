#include "Command.h"

#include "../metrics/Metric.h"
#include "../metrics/MetricVisitor.h"
#include "impl/CommandImpl.h"

void Command::Impl::Accept(MetricVisitor &visitor) const {
  VISIT(visitor, progress_phase_);
  VISIT(visitor, progress_total_bytes_);
  VISIT(visitor, progress_current_bytes_);
  VISIT(visitor, progress_compressed_bytes_);
};

Command::Command() : pImpl(std::make_unique<Impl>()) {}

Command::Command(Command &&) noexcept = default;

Command::~Command() = default;

void Command::Accept(MetricVisitor &visitor) const {
  return pImpl->Accept(visitor);
}
