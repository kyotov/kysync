#ifndef KSYNC_SRC_COMMANDS_INCLUDE_COMMAND_H
#define KSYNC_SRC_COMMANDS_INCLUDE_COMMAND_H

#include <ky/metrics/metrics.h>
#include <ky/observability/observable.h>

namespace kysync {

class Command : public ky::observability::Observable,
                public ky::metrics::MetricContainer {
public:
  Command(std::string name);

  virtual int Run() = 0;
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_INCLUDE_COMMAND_H
