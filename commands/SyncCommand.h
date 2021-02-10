#ifndef KSYNC_SYNCCOMMAND_H
#define KSYNC_SYNCCOMMAND_H

#include <filesystem>

#include "../readers/Reader.h"
#include "Command.h"

class SyncCommand final : public Command {
public:
  explicit SyncCommand(
      const Reader &metadataReader,
      const Reader &dataReader,
      const Reader &seedReader,
      const std::filesystem::path &outputPath);

  ~SyncCommand();

  int run() override;

  void accept(MetricVisitor &visitor) const override;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;

  friend class KySyncTest;
};

#endif  // KSYNC_SYNCCOMMAND_H
