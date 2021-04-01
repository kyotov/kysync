#include "Command.h"

#include "../metrics/Metric.h"
#include "../metrics/MetricVisitor.h"
#include "impl/CommandImpl.h"

void Command::Impl::accept(MetricVisitor &visitor) const
{
  VISIT(visitor, progressPhase);
  VISIT(visitor, progressTotalBytes);
  VISIT(visitor, progressCurrentBytes);
  VISIT(visitor, progress_compressed_bytes_);
};

Command::Command() : pImpl(std::make_unique<Impl>()) {}

Command::Command(Command &&) noexcept = default;

Command::~Command() = default;

void Command::accept(MetricVisitor &visitor) const
{
  return pImpl->accept(visitor);
}
