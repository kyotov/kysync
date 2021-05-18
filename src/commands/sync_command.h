#ifndef KSYNC_SYNC_COMMAND_H
#define KSYNC_SYNC_COMMAND_H

#include <filesystem>

#include "../readers/reader.h"
#include "command.h"

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
      int num_blocks_in_batch,
      int threads);

  ~SyncCommand() override;

  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SYNC_COMMAND_H
