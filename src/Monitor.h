#ifndef KSYNC_MONITOR_H
#define KSYNC_MONITOR_H

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
};

}  // namespace kysync

#endif  // KSYNC_MONITOR_H
