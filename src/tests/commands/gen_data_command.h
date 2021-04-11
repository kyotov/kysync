#ifndef KSYNC_GEN_DATA_COMMAND_H
#define KSYNC_GEN_DATA_COMMAND_H

#include <filesystem>
#include <utility>

#include "../../commands/command.h"
#include "../../utilities/utilities.h"

namespace kysync {

class GenDataCommand final : public Command {
  PIMPL;
  NO_COPY_OR_MOVE(GenDataCommand);

public:
  GenDataCommand(
      const std::filesystem::path &output_path,
      uint64_t data_size,
      uint64_t seed_data_size,
      uint64_t fragment_size,
      uint32_t similarity);

  ~GenDataCommand() noexcept final;

  int Run() override;
};

}  // namespace kysync

#endif  // KSYNC_GEN_DATA_COMMAND_H
