#ifndef KSYNC_SRC_TESTS_COMMANDS_SYSTEM_COMMAND_H
#define KSYNC_SRC_TESTS_COMMANDS_SYSTEM_COMMAND_H

#include <ky/observability/observable.h>

#include <string>

namespace kysync {

class SystemCommand final : public ky::observability::Observable {
  std::string command_;

public:
  explicit SystemCommand(std::string name, std::string command);

  int Run();
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_COMMANDS_SYSTEM_COMMAND_H
