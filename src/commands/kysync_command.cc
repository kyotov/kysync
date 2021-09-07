#include <kysync/commands/kysync_command.h>

namespace kysync {

KySyncCommand::KySyncCommand(std::string name)
    : Command(std::move(name)) {}

}  // namespace kysync