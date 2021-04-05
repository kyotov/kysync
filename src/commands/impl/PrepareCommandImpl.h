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

  std::istream& input_;
  std::ostream& output_ksync_;
  std::ostream& output_compressed_;
  const size_t block_size_;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<uint64_t> compressed_sizes_;
  const int compression_level_ = 1;

  Metric compressed_bytes_;

  Impl(
      const PrepareCommand &parent,
      std::istream& input,
      std::ostream& output_ksync,
      std::ostream& output_compressed,
      size_t block_size);

  template <typename T>
  void WriteToMetadataStream(const std::vector<T>& container);

  int Run();

  void Accept(MetricVisitor& visitor, const PrepareCommand& host);
};

}  // namespace kysync

#endif  // KSYNC_PREPARECOMMANDIMPL_H
