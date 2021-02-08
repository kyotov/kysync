#ifndef KSYNC_PREPARECOMMAND_H
#define KSYNC_PREPARECOMMAND_H

#include <memory>

#include "../readers/Reader.h"
#include "Command.h"

class KySyncTest;

class PrepareCommand final : public Command {
public:
  explicit PrepareCommand(
      const Reader &reader,
      size_t block,
      std::ostream &output);

  PrepareCommand(PrepareCommand &&) = default;

  ~PrepareCommand();

  // TODO: can this be const??
  int run() override;

  void accept(MetricVisitor &visitor) const override;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;

  friend class KySyncTest;
};

#endif  // KSYNC_PREPARECOMMAND_H
