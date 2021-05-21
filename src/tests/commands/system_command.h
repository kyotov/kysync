#ifndef KSYNC_SYSTEM_COMMAND_H
#define KSYNC_SYSTEM_COMMAND_H

#include <string>
#include <vector>

#include "../../commands/command.h"
#include "../../utilities/utilities.h"

namespace kysync {

class SystemCommand final : public Command {
  PIMPL;
  NO_COPY_OR_MOVE(SystemCommand);

public:
  explicit SystemCommand(std::string command);

  ~SystemCommand() noexcept final;

  int Run();
};

}  // namespace kysync

#endif  // KSYNC_SYSTEM_COMMAND_H
