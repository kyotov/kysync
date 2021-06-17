#ifndef KSYNC_SRC_COMMANDS_INCLUDE_KYSYNC_COMMANDS_PREPARE_COMMAND_H
#define KSYNC_SRC_COMMANDS_INCLUDE_KYSYNC_COMMANDS_PREPARE_COMMAND_H

#include <kysync/commands/kysync_command.h>

#include <filesystem>
#include <memory>

namespace kysync {

class PrepareCommand : public KySyncCommand {
protected:
  PrepareCommand();

public:
  static std::unique_ptr<PrepareCommand> Create(
      std::filesystem::path input_file_path,
      std::filesystem::path output_ksync_file_path,
      std::filesystem::path output_compressed_file_path,
      std::streamsize block_size,
      int threads);
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_INCLUDE_KYSYNC_COMMANDS_PREPARE_COMMAND_H
