#ifndef KSYNC_SRC_COMMANDS_SYNC_COMMAND_H
#define KSYNC_SRC_COMMANDS_SYNC_COMMAND_H

#include <ky/observability/observable.h>
#include <kysync/checksums/strong_checksum.h>
#include <kysync/commands/kysync_command.h>
#include <kysync/readers/reader.h>

#include <filesystem>

namespace kysync {

class SyncCommand : public KySyncCommand {
  friend class KySyncTest;

  virtual void ReadMetadata() = 0;
  virtual std::vector<std::streamoff> GetTestAnalysis() const = 0;

protected:
  SyncCommand();

public:
  virtual ~SyncCommand() = default;

  static std::unique_ptr<SyncCommand> Create(
      const std::string &data_uri,
      const std::string &metadata_uri,
      std::string seed_uri,
      std::filesystem::path output_path,
      bool compression_disabled,
      int num_blocks_in_batch,
      int threads);
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_SYNC_COMMAND_H