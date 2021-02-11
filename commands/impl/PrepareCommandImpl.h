#ifndef KSYNC_PREPARECOMMANDIMPL_H
#define KSYNC_PREPARECOMMANDIMPL_H

#include <vector>

#include "../../checksums/scs.h"
#include "../../metrics/Metric.h"
#include "../../metrics/MetricVisitor.h"
#include "../../readers/Reader.h"
#include "../PrepareCommand.h"

class PrepareCommand::Impl final {
  friend class PrepareCommand;
  friend class KySyncTest;

  std::istream& input;
  std::ostream& output;
  const size_t block;

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;

  Metric progressPhase;
  Metric progressTotalBytes;
  Metric progressCurrentBytes;

  Impl(std::istream& _input, std::ostream& _output, size_t _block);

  int run();

  void accept(MetricVisitor& visitor, const PrepareCommand& host);
};

#endif  // KSYNC_PREPARECOMMANDIMPL_H
