#ifndef KSYNC_SYNCCOMMANDIMPL_H
#define KSYNC_SYNCCOMMANDIMPL_H

#include <bitset>

#include "../../checksums/StrongChecksum.h"
#include "../SyncCommand.h"
#include "CommandImpl.h"

class SyncCommand::Impl final {
  friend class SyncCommand;
  friend class KySyncTest;

  const std::string dataUri;
  const std::string metadataUri;
  const std::string seedUri;
  const std::filesystem::path outputPath;

  Command::Impl &baseImpl;

  Metric weakChecksumMatches;
  Metric weakChecksumFalsePositive;
  Metric strongChecksumMatches;

  Metric reusedBytes;
  Metric downloadedBytes;

  size_t size{};
  size_t headerSize{};
  size_t block{};
  size_t blockCount{};

  std::string hash;

  std::vector<uint32_t> weakChecksums;
  std::vector<StrongChecksum> strongChecksums;
  // NOTE: This attempts to use the new style despite inconsistency
  std::vector<uint64_t> compressed_sizes_;
  std::vector<uint64_t> compressed_file_offsets_;

  const int threads;

  struct WcsMapData {
    size_t index{};
    size_t seedOffset{-1ull};
  };

  std::bitset<0x100000000ull> set;
  std::unordered_map<uint32_t, WcsMapData> analysis;

  Impl(
      std::string _dataUri,
      std::string _metadataUri,
      std::string _seedUri,
      std::filesystem::path _outputPath,
      int _threads,
      Command::Impl &_baseImpl);

  template<typename T>
  size_t ReadMetadataIntoContainer(const Reader& metadata_reader, size_t offset, std::vector<T>& container);

  void parseHeader(const Reader &metadataReader);
  void readMetadata();
  void analyzeSeedChunk(int id, size_t startOffset, size_t endOffset);
  void analyzeSeed();
  void reconstructSourceChunk(int id, size_t startOffset, size_t endOffset);
  void reconstructSource();

  int run();

  void accept(MetricVisitor &visitor, const SyncCommand &host);
};

#endif  // KSYNC_SYNCCOMMANDIMPL_H
