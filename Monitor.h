#ifndef KSYNC_MONITOR_H
#define KSYNC_MONITOR_H

#include <memory>

#include "commands/Command.h"

namespace kysync {

class Monitor final {
public:
  explicit Monitor(Command &);

  ~Monitor();

  int Run();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kysync

#endif  // KSYNC_MONITOR_H
