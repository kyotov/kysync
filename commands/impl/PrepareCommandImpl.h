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

class PrepareCommand::Impl final {
  friend class PrepareCommand;
  friend class KySyncTest;

  std::istream& input;
  std::ostream& output_ksync_;
  std::ostream& output_compressed_;
  const size_t block;

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;
  // NOTE: This attempts to use the new style despite inconsistency
  std::vector<uint64_t> compressed_sizes_;
  const int compression_level_ = 1;

  Command::Impl &baseImpl;

  Impl(
      std::istream& _input,
      std::ostream& _output_ksync,
      std::ostream& _output_compressed,
      size_t _block,
      Command::Impl& _baseImpl);

  int run();

  void accept(MetricVisitor& visitor, const PrepareCommand& host);
};

#endif  // KSYNC_PREPARECOMMANDIMPL_H
