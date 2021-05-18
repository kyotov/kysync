#ifndef KSYNC_SYNC_COMMAND_IMPL_H
#define KSYNC_SYNC_COMMAND_IMPL_H

#include <bitset>
#include <ios>
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
  const int kNumBlocksPerRetrieval;
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
      int num_blocks_in_batch,
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
  bool FoundMatchingSeedOffset(size_t block_index, size_t *matching_seed_offset)
      const;
  void ReconstructSource();

  int Run();

  void Accept(MetricVisitor &visitor);

  class ChunkReconstructor {
    SyncCommand::Impl &parent_impl_;

    std::unique_ptr<char[]> buffer_;
    std::unique_ptr<char[]> decompression_buffer_;
    std::unique_ptr<Reader> seed_reader_;
    std::unique_ptr<Reader> data_reader_;
    std::fstream output_;
    std::vector<BatchedRetrivalInfo> batched_retrieval_infos_;

    std::fstream GetOutputStream(size_t start_offset);
    void WriteRetrievedBatch(size_t size_to_write);
    void ValidateAndWrite(size_t block_index, const char *buffer, size_t count);
    size_t Decompress(
        size_t compressed_size,
        const void *decompression_buffer,
        void *output_buffer) const;

  public:
    ChunkReconstructor(
        SyncCommand::Impl &parent,
        size_t start_offset,
        size_t end_offset);
    void ReconstructFromSeed(size_t block_index, size_t seed_offset);
    void EnqueueBlockRetrieval(size_t block_index, size_t begin_offset);
    void FlushBatch(bool force);
  };
};

}  // namespace kysync

#endif  // KSYNC_SYNC_COMMAND_IMPL_H
