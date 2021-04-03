#ifndef KSYNC_COMMAND_H
#define KSYNC_COMMAND_H

#include <memory>

#include "../metrics/MetricContainer.h"

class Command : public MetricContainer {
public:
  Command();

  Command(Command &&) noexcept;

  virtual ~Command();

  virtual int Run() = 0;

  void Accept(MetricVisitor &visitor) const override;

protected:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // KSYNC_COMMAND_H
