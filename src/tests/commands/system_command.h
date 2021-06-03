#ifndef KSYNC_SYSTEM_COMMAND_H
#define KSYNC_SYSTEM_COMMAND_H

#include <string>
#include <vector>

#include "../../commands/command.h"

namespace kysync {

class SystemCommand final : public Command {
  std::string command_;

public:
  explicit SystemCommand(std::string command);

  int Run() override;
};

}  // namespace kysync

#endif  // KSYNC_SYSTEM_COMMAND_H
