#ifndef KSYNC_COMMAND_H
#define KSYNC_COMMAND_H

#include "../metrics/MetricContainer.h"

class Command : public MetricContainer {
public:
  virtual int run() = 0;
};

#endif  // KSYNC_COMMAND_H
