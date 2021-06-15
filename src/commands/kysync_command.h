#ifndef KSYNC_SRC_COMMANDS_KYSYNCCOMMAND_H
#define KSYNC_SRC_COMMANDS_KYSYNCCOMMAND_H

#include <vector>

#include "../checksums/strong_checksum.h"
#include "command.h"

namespace kysync {

class KySyncCommand : public Command {
  friend class KySyncTest;

  virtual const std::vector<uint32_t> &GetWeakChecksums() const = 0;
  virtual const std::vector<StrongChecksum> &GetStrongChecksums() const = 0;
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_KYSYNCCOMMAND_H
