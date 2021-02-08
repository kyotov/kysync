#ifndef KSYNC_SYNCCOMMAND_H
#define KSYNC_SYNCCOMMAND_H

#include "Command.h"
#include "../readers/Reader.h"

class SyncCommand final : public Command {
public:
  explicit SyncCommand(const Reader &metadataReader, const Reader &dataReader);

  ~SyncCommand();

  int run() override;

  void accept(MetricVisitor &visitor) const override;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;

  friend class KySyncTest;
};

#endif  // KSYNC_SYNCCOMMAND_H
