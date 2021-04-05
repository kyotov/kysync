#ifndef KSYNC_SYNCCOMMAND_H
#define KSYNC_SYNCCOMMAND_H

#include <filesystem>

#include "../readers/Reader.h"
#include "Command.h"

namespace kysync {

class SyncCommand final : public Command {
  friend class KySyncTest;
  PIMPL;
  NO_COPY_OR_MOVE(SyncCommand);

public:
  explicit SyncCommand(
      const std::string &data_uri,
      const std::string &metadata_uri,
      const std::string &seed_uri,
      const std::filesystem::path &output_path,
      bool compression_disabled,
      int threads);

  ~SyncCommand() override;

  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SYNCCOMMAND_H
