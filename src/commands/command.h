#ifndef KSYNC_COMMAND_H
#define KSYNC_COMMAND_H

#include <memory>

#include "../metrics/metric.h"
#include "../metrics/metric_container.h"
#include "../utilities/utilities.h"

namespace kysync {

class Command : public MetricContainer {
  PIMPL;

public:
  Command();

  virtual ~Command();

  virtual int Run() = 0;

  void Accept(MetricVisitor &visitor) override;

  void StartNextPhase(MetricValueType size) const;
  MetricValueType AdvanceProgress(  // NOLINT{modernize-use-nodiscard}
      MetricValueType amount) const;
};

}  // namespace kysync

#endif  // KSYNC_COMMAND_H
