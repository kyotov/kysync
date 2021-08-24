#include <kysync/commands/command.h>

namespace kysync {

Command::Command(std::string name)
    : ky::observability::Observable(std::move(name)) {}

}  // namespace kysync