#ifndef KSYNC_PREPARECOMMAND_H
#define KSYNC_PREPARECOMMAND_H

#include <filesystem>
#include <memory>

#include "../readers/Reader.h"
#include "Command.h"

namespace fs = std::filesystem;

class KySyncTest;

class PrepareCommand final : public Command {
public:
  PrepareCommand(
      std::istream &input,
      std::ostream &output,
      size_t block);

  PrepareCommand(PrepareCommand &&) noexcept;

  ~PrepareCommand() override;

  // TODO: can this be const??
  int run() override;

  void accept(MetricVisitor &visitor) const override;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;

  friend class KySyncTest;
};

#endif  // KSYNC_PREPARECOMMAND_H
