#ifndef KSYNC_PREPARECOMMAND_H
#define KSYNC_PREPARECOMMAND_H

#include <memory>

#include "../readers/Reader.h"
#include "Command.h"

class KySyncTest;

class PrepareCommand final : public Command {
public:
  explicit PrepareCommand(Reader &reader, size_t block);

  ~PrepareCommand();

  int run() override;

  void accept(MetricVisitor &visitor) override;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;

  friend class KySyncTest;
};

#endif  // KSYNC_PREPARECOMMAND_H
