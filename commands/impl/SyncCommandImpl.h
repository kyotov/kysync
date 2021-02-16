#ifndef KSYNC_SYNCCOMMANDIMPL_H
#define KSYNC_SYNCCOMMANDIMPL_H

#include <bitset>

#include "../../checksums/StrongChecksum.h"
#include "../SyncCommand.h"

class SyncCommand::Impl final {
  friend class SyncCommand;
  friend class KySyncTest;

  std::unique_ptr<Reader> dataReader;
  std::unique_ptr<Reader> metadataReader;
  std::string seedUri;
  std::ostream &output;

  Metric progressPhase;
  Metric progressTotalBytes;
  Metric progressCurrentBytes;

  Metric weakChecksumMatches;
  Metric weakChecksumFalsePositive;
  Metric strongChecksumMatches;

  Metric reusedBytes;

  size_t size{};
  size_t headerSize{};
  size_t block{};
  size_t blockCount{};

  std::string hash;

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;

  struct WcsMapData {
    size_t index{};
    size_t seedOffset{-1ull};
  };

  std::bitset<0x100000000ull> set;
  std::unordered_map<uint32_t, WcsMapData> analysis;

  Impl(
      const std::string &data_uri,
      const std::string &metadata_uri,
      std::string seed_uri,
      std::ostream &_output);

  void parseHeader();
  void readMetadata();
  //TODO: decide if we want to keep this signature... reader is only for debug
  void analyzeSeedCallback(
      const char *buffer,
      size_t offset,
      uint32_t wcs,
      size_t seedOffset,
      const Reader &seedReader);
  void analyzeSeedChunk(size_t startOffset, size_t endOffset);
  void analyzeSeed();
  void reconstructSource();

  int run();

  void accept(MetricVisitor &visitor, const SyncCommand &host);
};

#endif  // KSYNC_SYNCCOMMANDIMPL_H
