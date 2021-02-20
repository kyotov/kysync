#ifndef KSYNC_COMMAND_H
#define KSYNC_COMMAND_H

#include "../metrics/MetricContainer.h"

#include <memory>

class Command : public MetricContainer {
public:
  Command();

  Command(Command &&) noexcept;

  virtual ~Command();

  virtual int run() = 0;

  void accept(MetricVisitor &visitor) const override;

protected:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_COMMAND_H
