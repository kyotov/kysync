#ifndef KSYNC_SYNCCOMMANDIMPL_H
#define KSYNC_SYNCCOMMANDIMPL_H

#include <bitset>
#include <unordered_map>

#include "../../checksums/StrongChecksum.h"
#include "../SyncCommand.h"
#include "CommandImpl.h"

namespace kysync {

class SyncCommand::Impl final {
  friend class SyncCommand;
  friend class KySyncTest;

  const std::string data_uri_;
  const bool compression_diabled_;
  const std::string metadata_uri_;
  const std::string seed_uri_;
  const std::filesystem::path output_path_;

  Command::Impl &base_impl_;

  Metric weak_checksum_matches_;
  Metric weak_checksum_false_positive_;
  Metric strong_checksum_matches_;

  Metric reused_bytes_;
  Metric downloaded_bytes_;

  size_t size_{};
  size_t header_size_{};
  size_t block_{};
  size_t block_count_{};
  size_t max_compressed_size_{};

  std::string hash_;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<size_t> compressed_sizes_;
  std::vector<size_t> compressed_file_offsets_;

  const int threads_;

  struct WcsMapData {
    size_t index{};
    size_t seed_offset{-1ULL};
  };

  static constexpr auto k4Gb = 0x100000000ULL;

  std::bitset<k4Gb> set_;
  std::unordered_map<uint32_t, WcsMapData> analysis_;

  Impl(
      std::string data_uri,
      bool compression_diabled,
      std::string metadata_uri,
      std::string seed_uri,
      std::filesystem::path output_path,
      int threads,
      Command::Impl &base_impl);

  template <typename T>
  size_t ReadIntoContainer(
      const Reader &metadata_reader,
      size_t offset,
      std::vector<T> &container);

  void ParseHeader(const Reader &metadata_reader);
  void UpdateCompressedOffsetsAndMaxSize();
  void ReadMetadata();
  void AnalyzeSeedChunk(int id, size_t start_offset, size_t end_offset);
  void AnalyzeSeed();
  void ReconstructSourceChunk(int id, size_t start_offset, size_t end_offset);
  void ReconstructSource();

  int Run();

  void Accept(MetricVisitor &visitor);
};

}  // namespace kysync

#endif  // KSYNC_SYNCCOMMANDIMPL_H
