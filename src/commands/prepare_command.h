#ifndef KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H
#define KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H

#include <filesystem>
#include <memory>
#include <vector>

#include "../checksums/strong_checksum.h"
#include "../readers/reader.h"
#include "command.h"

namespace kysync {

namespace fs = std::filesystem;

class KySyncTest;

class PrepareCommand final : public Command {
  friend class KySyncTest;

  fs::path input_file_path_;
  fs::path output_ksync_file_path_;
  fs::path output_compressed_file_path_;
  std::streamsize block_size_;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<std::streamsize> compressed_sizes_;
  int compression_level_ = 1;

  Metric compressed_bytes_{};

public:
  PrepareCommand(
      fs::path input_file_path,
      fs::path output_ksync_file_path,
      fs::path output_compressed_file_path,
      std::streamsize block_size);

  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_PREPARE_COMMAND_H
