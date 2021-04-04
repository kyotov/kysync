#ifndef KSYNC_SYNCCOMMAND_H
#define KSYNC_SYNCCOMMAND_H

#include <filesystem>

#include "../readers/Reader.h"
#include "Command.h"

class SyncCommand final : public Command {
public:
  explicit SyncCommand(
      std::string data_uri,
      bool compression_disabled,
      std::string metadata_uri,
      std::string seed_uri,
      std::filesystem::path output_path,
      int threads);

  ~SyncCommand() override;

  int Run() override;

  void Accept(MetricVisitor &visitor) const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  friend class KySyncTest;
};

#endif  // KSYNC_SYNCCOMMAND_H
