#ifndef KSYNC_SYNCCOMMANDIMPL_H
#define KSYNC_SYNCCOMMANDIMPL_H

#include <bitset>

#include "../../checksums/scs.h"
#include "../SyncCommand.h"

class SyncCommand::Impl final {
  friend class SyncCommand;
  friend class KySyncTest;

  const Reader &metadataReader;
  const Reader &dataReader;
  const Reader &seedReader;

  Metric progressTotalBytes;
  Metric progressCurrentBytes;

  Metric weakChecksumMatches;
  Metric weakChecksumFalsePositive;
  Metric strongChecksumMatches;

  Metric size;
  size_t headerSize{};
  size_t block{};
  size_t blockCount{};

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;

  struct WcsMapData {
    size_t index{};
    size_t seedOffset{-1ull};
  };

  std::unordered_map<uint32_t, WcsMapData> analysis;

  Impl(
      const Reader &_metadataReader,
      const Reader &_dataReader,
      const Reader &_seedReader);

  void parseHeader();
  void readMetadata();
  void analyzeSeed();
  void analyzeSeedCallback(const char *buffer, size_t offset, uint32_t wcs);

  int run();

  void accept(MetricVisitor &visitor, const SyncCommand &host);
};

#endif  // KSYNC_SYNCCOMMANDIMPL_H
