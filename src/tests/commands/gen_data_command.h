#ifndef KSYNC_SRC_TESTS_COMMANDS_GEN_DATA_COMMAND_H
#define KSYNC_SRC_TESTS_COMMANDS_GEN_DATA_COMMAND_H

#include <filesystem>
#include <utility>

#include "../../commands/command.h"

namespace kysync {

class GenDataCommand final : public Command {
  std::filesystem::path path_;
  std::filesystem::path data_file_path_;
  std::filesystem::path seed_data_file_path_;
  std::streamsize data_size_;
  std::streamsize seed_data_size_;
  std::streamsize fragment_size_;
  std::streamsize diff_size_;

  void GenChunk(int id, std::streamoff beg, std::streamoff end);

public:
  GenDataCommand(
      const std::filesystem::path &output_path,
      std::streamsize data_size,
      std::streamsize seed_data_size,
      std::streamsize fragment_size,
      int similarity);

  int Run() override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_COMMANDS_GEN_DATA_COMMAND_H
