#include <kysync/test_commands/system_command.h>

#include <memory>
#include <string>
#include <vector>

namespace kysync {

SystemCommand::SystemCommand(std::string name, std::string command)
    : Observable(std::move(name)),
      command_(std::move(command)) {}

int SystemCommand::Run() {
  StartNextPhase(1);
  auto result =
      system(command_.c_str());  // NOLINT(cert-env33-c, concurrency-mt-unsafe)
  StartNextPhase(0);
  return result;
}

}  // namespace kysync
