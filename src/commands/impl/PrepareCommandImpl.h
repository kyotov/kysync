#ifndef KSYNC_PREPARECOMMANDIMPL_H
#define KSYNC_PREPARECOMMANDIMPL_H

#include <vector>

// TODO: this should be moved to the cpp file
#include "../../checksums/StrongChecksum.h"
#include "../../metrics/Metric.h"
#include "../../metrics/MetricVisitor.h"
#include "../../readers/Reader.h"
#include "../PrepareCommand.h"
#include "CommandImpl.h"

namespace kysync {

class PrepareCommand::Impl final {
  friend class PrepareCommand;
  friend class KySyncTest;

  const PrepareCommand& kParent;

  fs::path input_filename_;
  fs::path output_ksync_filename_;
  fs::path output_compressed_filename_;
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

#endif  // KSYNC_PREPARECOMMANDIMPL_H
