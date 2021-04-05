#ifndef KSYNC_COMMAND_H
#define KSYNC_COMMAND_H

#include <memory>

#include "../metrics/Metric.h"
#include "../metrics/MetricContainer.h"

namespace kysync {

class Command : public MetricContainer {
public:
  Command();

  Command(Command &&) noexcept;

  virtual ~Command();

  virtual int Run() = 0;

  void Accept(MetricVisitor &visitor) override;

  void StartNextPhase(MetricValueType size) const;
  MetricValueType AdvanceProgress(  // NOLINT{modernize-use-nodiscard}
      MetricValueType amount) const;

protected:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kysync

#endif  // KSYNC_COMMAND_H
