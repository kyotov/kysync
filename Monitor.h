#ifndef KSYNC_MONITOR_H
#define KSYNC_MONITOR_H

#include <memory>

#include "commands/Command.h"

class Monitor final {
public:
  explicit Monitor(Command &);

  ~Monitor();

  int Run();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // KSYNC_MONITOR_H
