#ifndef KSYNC_SRC_KYSYNC_COMMANDS_INCLUDE_KYSYNC_COMMANDS_KYSYNC_COMMAND_H
#define KSYNC_SRC_KYSYNC_COMMANDS_INCLUDE_KYSYNC_COMMANDS_KYSYNC_COMMAND_H

#include <ky/metrics/metrics.h>
#include <ky/observability/observable.h>
#include <kysync/checksums/strong_checksum.h>
#include <kysync/commands/command.h>

#include <vector>

namespace kysync {

class KySyncCommand : public Command {
  friend class KySyncTest;

  virtual const std::vector<uint32_t> &GetWeakChecksums() const = 0;
  virtual const std::vector<StrongChecksum> &GetStrongChecksums() const = 0;

public:
  KySyncCommand(std::string name);
};

}  // namespace kysync

#endif  // KSYNC_SRC_KYSYNC_COMMANDS_INCLUDE_KYSYNC_COMMANDS_KYSYNC_COMMAND_H
