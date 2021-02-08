#ifndef KSYNC_SYNCCOMMANDIMPL_H
#define KSYNC_SYNCCOMMANDIMPL_H

#include "../SyncCommand.h"
#include "../../checksums/scs.h"

class SyncCommand::Impl final {
  friend class SyncCommand;
  friend class KySyncTest;

  const Reader &metadataReader;
  const Reader &dataReader;

  Metric progressTotalBytes;
  Metric progressCurrentBytes;

  Metric size;
  size_t headerSize{};
  size_t block{};
  size_t blockCount{};

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;

  Impl(const Reader &_metadataReader, const Reader &_dataReader);

  void parseHeader();
  void readMetadata();

  int run();

  void accept(MetricVisitor &visitor, const SyncCommand &host);
};

#endif  // KSYNC_SYNCCOMMANDIMPL_H
