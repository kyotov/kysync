#ifndef KSYNC_MONITOR_H
#define KSYNC_MONITOR_H

#include <memory>

#include "commands/Command.h"

class Monitor final {
public:
  explicit Monitor(Command &);

  ~Monitor();

  int run();

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_MONITOR_H
