#ifndef KSYNC_PREPARE_COMMAND_IMPL_H
#define KSYNC_PREPARE_COMMAND_IMPL_H

#include <vector>

// TODO: this should be moved to the cpp file
#include "../../checksums/strong_checksum.h"
#include "../../metrics/metric.h"
#include "../../metrics/metric_visitor.h"
#include "../../readers/reader.h"
#include "../prepare_command.h"
#include "command_impl.h"

namespace kysync {

class PrepareCommand::Impl final {
  friend class PrepareCommand;
  friend class KySyncTest;

  const PrepareCommand& kParent;

  const fs::path kInputFilename;
  const fs::path kOutputKsyncFilename;
  const fs::path kOutputCompressedFilename;
  const size_t kBlockSize;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<uint64_t> compressed_sizes_;
  const int kCompressionLevel = 1;

  Metric compressed_bytes_;

  Impl(
      const PrepareCommand &parent,
      fs::path input_filename,
      fs::path output_ksync_filename,
      fs::path output_compressed_filename,
      size_t block_size);

  int Run();

  void Accept(MetricVisitor& visitor, const PrepareCommand& host);
};

}  // namespace kysync

#endif  // KSYNC_PREPARE_COMMAND_IMPL_H
