#ifndef KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H
#define KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H

#include <filesystem>
#include <memory>
#include <vector>

#include "../checksums/strong_checksum.h"
#include "../readers/reader.h"
#include "command.h"

namespace kysync {

class KySyncTest;
class ChunkPreparer;

class PrepareCommand final : public Command {
  friend class KySyncTest;
  // TODO(kyotov): discuss with ashish the cost-benefit of this friendship :)
  friend class ChunkPreparer;

  std::filesystem::path input_file_path_;
  std::filesystem::path output_ksync_file_path_;
  std::filesystem::path output_compressed_file_path_;

  std::streamsize block_size_;
  std::streamsize max_compressed_block_size_;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<std::streamsize> compressed_sizes_;

  int compression_level_ = 1;
  int threads_;

  Metric compressed_bytes_{};

public:
  PrepareCommand(
      std::filesystem::path input_file_path,
      std::filesystem::path output_ksync_file_path,
      std::filesystem::path output_compressed_file_path,
      std::streamsize block_size,
      int threads);

  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H
