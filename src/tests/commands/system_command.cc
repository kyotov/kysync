#include "system_command.h"

#include <memory>
#include <string>
#include <vector>

namespace kysync {

struct SystemCommand::Impl {
  const std::string kCommand;

  Impl(std::string command) : kCommand(std::move(command)) {}
};

SystemCommand::SystemCommand(std::string command)
    : impl_(std::make_unique<Impl>(std::move(command))) {}

SystemCommand::~SystemCommand() noexcept = default;

int SystemCommand::Run() { return system(impl_->kCommand.c_str()); }

}  // namespace kysync
