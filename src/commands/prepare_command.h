#ifndef KSYNC_PREPARE_COMMAND_H
#define KSYNC_PREPARE_COMMAND_H

#include <filesystem>
#include <memory>
#include <vector>

#include "../readers/reader.h"
#include "command.h"
#include "../checksums/strong_checksum.h"

namespace kysync {

namespace fs = std::filesystem;

class KySyncTest;

class PrepareCommand final : public Command {
  friend class KySyncTest;

  const fs::path kInputFilename;
  const fs::path kOutputKsyncFilename;
  const fs::path kOutputCompressedFilename;
  const size_t kBlockSize;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<uint64_t> compressed_sizes_;
  const int kCompressionLevel = 1;

  Metric compressed_bytes_;

public:
  PrepareCommand(
      fs::path input_filename,
      fs::path output_ksync_filename,
      fs::path output_compressed_filename,
      size_t block_size);

  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_PREPARE_COMMAND_H
