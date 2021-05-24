#ifndef KSYNC_GEN_DATA_COMMAND_H
#define KSYNC_GEN_DATA_COMMAND_H

#include <filesystem>
#include <utility>

#include "../../commands/command.h"

namespace kysync {

class GenDataCommand final : public Command {
  const std::filesystem::path kPath;
  const std::filesystem::path kData;
  const std::filesystem::path kSeedData;
  const size_t kDataSize;
  const size_t kSeedDataSize;
  const size_t kFragmentSize;
  const size_t kDiffSize;

  void GenChunk(int id, size_t beg, size_t end);

public:
  GenDataCommand(
      const std::filesystem::path &output_path,
      uint64_t data_size,
      uint64_t seed_data_size,
      uint64_t fragment_size,
      uint32_t similarity);

  int Run() override;
};

}  // namespace kysync

#endif  // KSYNC_GEN_DATA_COMMAND_H
