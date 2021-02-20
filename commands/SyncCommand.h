#ifndef KSYNC_SYNCCOMMAND_H
#define KSYNC_SYNCCOMMAND_H

#include <filesystem>

#include "../readers/Reader.h"
#include "Command.h"

class SyncCommand final : public Command {
public:
  explicit SyncCommand(
      std::string dataUri,
      std::string metadataUri,
      std::string seedUri,
      std::filesystem::path outputPath,
      int threads);

  ~SyncCommand() override;

  int run() override;

  void accept(MetricVisitor &visitor) const override;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;

  friend class KySyncTest;
};

#endif  // KSYNC_SYNCCOMMAND_H
