#ifndef KSYNC_MONITOR_H
#define KSYNC_MONITOR_H

#include <functional>
#include <memory>

#include "commands/Command.h"
#include "utilities/utilities.h"

namespace kysync {

class Monitor final {
  PIMPL;
  NO_COPY_OR_MOVE(Monitor);

public:
  explicit Monitor(Command &);

  ~Monitor();

  int Run();

  using MetricCallback = std::function<void(std::string, MetricValueType)>;
  void MetricSnapshot(const MetricCallback &callback) const;
};

}  // namespace kysync

#endif  // KSYNC_MONITOR_H
