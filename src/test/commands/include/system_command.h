#ifndef KSYNC_SRC_TESTS_COMMANDS_SYSTEM_COMMAND_H
#define KSYNC_SRC_TESTS_COMMANDS_SYSTEM_COMMAND_H

#include <kysync/commands/command.h>

#include <string>

namespace kysync {

class SystemCommand final : public Command {
  std::string command_;

public:
  SystemCommand(std::string name, std::string command);

  int Run() override;

  void Accept(ky::metrics::MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_COMMANDS_SYSTEM_COMMAND_H
