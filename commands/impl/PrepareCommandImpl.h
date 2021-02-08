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

  const Reader &reader;
  const size_t block;
  std::ostream &output;

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;

  Metric progressTotalBytes;
  Metric progressCurrentBytes;

  Impl(const Reader &_reader, size_t _block, std::ostream &output);

  int run();

  void accept(MetricVisitor &visitor, const PrepareCommand &host);
};

#endif  // KSYNC_PREPARECOMMANDIMPL_H
