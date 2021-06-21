#ifndef KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H
#define KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H

#include <filesystem>
#include <memory>

#include "kysync_command.h"

namespace kysync {

class PrepareCommand : public KySyncCommand {
public:
  virtual ~PrepareCommand() = default;

  static std::unique_ptr<PrepareCommand> Create(
      std::filesystem::path input_file_path,
      std::filesystem::path output_ksync_file_path,
      std::filesystem::path output_compressed_file_path,
      std::streamsize block_size,
      int threads);
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H
