#ifndef KSYNC_SRC_COMMANDS_KYSYNC_COMMAND_H
#define KSYNC_SRC_COMMANDS_KYSYNC_COMMAND_H

#include <ky/metrics/metrics.h>
#include <ky/observability/observable.h>
#include <kysync/checksums/strong_checksum.h>

#include <vector>

namespace kysync {

class KySyncCommand : virtual public ky::observability::Observable,
                      public ky::metrics::MetricContainer {
  friend class KySyncTest;

  virtual const std::vector<uint32_t> &GetWeakChecksums() const = 0;
  virtual const std::vector<StrongChecksum> &GetStrongChecksums() const = 0;

public:
  virtual int Run() = 0;
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_KYSYNC_COMMAND_H
