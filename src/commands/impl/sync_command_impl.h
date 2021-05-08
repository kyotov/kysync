#ifndef KSYNC_SYNC_COMMAND_IMPL_H
#define KSYNC_SYNC_COMMAND_IMPL_H

#include <bitset>
#include <unordered_map>

#include "../../checksums/strong_checksum.h"
#include "../sync_command.h"
#include "command_impl.h"

namespace kysync {

class SyncCommand::Impl final {
  friend class SyncCommand;
  friend class KySyncTest;

  const SyncCommand &kParent;
  const std::string kDataUri;
  const std::string kMetadataUri;
  const std::string kSeedUri;
  const std::filesystem::path kOutputPath;
  const bool kCompressionDiabled;
  const int kThreads;

  Metric weak_checksum_matches_;
  Metric weak_checksum_false_positive_;
  Metric strong_checksum_matches_;
  Metric reused_bytes_;
  Metric downloaded_bytes_;
  Metric decompressed_bytes_;

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

  struct WcsMapData {
    size_t index{};
    size_t seed_offset{-1ULL};
  };

  static constexpr auto k4Gb = 0x100000000ULL;

  std::bitset<k4Gb> set_;
  std::unordered_map<uint32_t, WcsMapData> analysis_;

  Impl(
      const SyncCommand &parent,
      const std::string &data_uri,
      const std::string &metadata_uri,
      const std::string &seed_uri,
      const std::filesystem::path &output_path,
      bool compression_disabled,
      int threads);

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
  std::fstream GetOutputStream(size_t start_offset);
  bool FoundMatchingSeedOffset(
      size_t block_index,
      size_t *matching_seed_offset);
  size_t RetrieveFromSource(
      size_t block_index,
      const Reader *data_reader,
      void *decompression_buffer,
      void *buffer);
  size_t RetreiveFromCompressedSource(
      size_t block_index,
      const Reader *data_reader,
      void *decompression_buffer);
  size_t Decompress(
      size_t block_index,
      size_t compressed_size,
      const void *decompression_buffer,
      void *output_buffer);
  void Validate(size_t i, const void *buffer, size_t count);
  void ReconstructSource();

  int Run();

  void Accept(MetricVisitor &visitor);
};

}  // namespace kysync

#endif  // KSYNC_SYNC_COMMAND_IMPL_H
