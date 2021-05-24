#include "system_command.h"

#include <memory>
#include <string>
#include <vector>

namespace kysync {

SystemCommand::SystemCommand(std::string command)
    : kCommand(std::move(command)) {}

int SystemCommand::Run() { return system(kCommand.c_str()); }

}  // namespace kysync
