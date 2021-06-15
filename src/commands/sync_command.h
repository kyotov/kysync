#ifndef KSYNC_SRC_COMMANDS_SYNC_COMMAND_H
#define KSYNC_SRC_COMMANDS_SYNC_COMMAND_H

#include <bitset>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "../checksums/strong_checksum.h"
#include "../readers/reader.h"
#include "../utilities/file_stream.h"
#include "command.h"

namespace kysync {

// TODO(kyotov): discuss -- the user only really needs the constructor...
//  should this all be in the implementation with a abstract interface here?
class SyncCommand final : public Command {
  friend class KySyncTest;

  std::string data_uri_;
  std::string metadata_uri_;
  std::string seed_uri_;
  std::filesystem::path output_path_;
  bool compression_disabled_;
  int blocks_per_batch_;
  int threads_;

  Metric weak_checksum_matches_{};
  Metric weak_checksum_false_positive_{};
  Metric strong_checksum_matches_{};
  Metric reused_bytes_{};
  Metric downloaded_bytes_{};
  Metric decompressed_bytes_{};

  std::streamsize size_{};
  std::streamsize header_size_{};
  std::streamsize block_{};
  std::streamsize block_count_{};
  std::streamsize max_compressed_size_{};

  std::string hash_;

  std::vector<uint32_t> weak_checksums_;
  std::vector<StrongChecksum> strong_checksums_;
  std::vector<std::streamsize> compressed_sizes_;
  std::vector<std::streamoff> compressed_file_offsets_;

  struct WcsMapData {
    std::streamsize index{};
    std::streamoff seed_offset{-1};
  };

  static constexpr auto k4Gb = 0x100000000LL;

  std::unique_ptr<std::bitset<k4Gb>> set_;
  std::unordered_map<uint32_t, WcsMapData> analysis_;

  void ParseHeader(Reader &metadata_reader);
  void UpdateCompressedOffsetsAndMaxSize();
  void ReadMetadata();
  void AnalyzeSeedChunk(
      int id,
      std::streamoff start_offset,
      std::streamoff end_offset);

  void AnalyzeSeed();

  void ReconstructSourceChunk(
      int id,
      std::streamoff start_offset,
      std::streamoff end_offset,
      const FileStream &output_file_stream);

  std::streamoff FindSeedOffset(int block_index) const;
  void ReconstructSource();

  template <typename T>
  std::streamsize ReadIntoContainer(
      Reader &metadata_reader,
      std::streamoff offset,
      std::vector<T> &container);

  class ChunkReconstructor {
    SyncCommand &parent_impl_;

    std::vector<char> buffer_;
    std::vector<char> decompression_buffer_;
    std::unique_ptr<Reader> seed_reader_;
    std::unique_ptr<Reader> data_reader_;
    std::fstream output_;
    std::vector<BatchRetrivalInfo> batched_retrieval_infos_;

    void WriteRetrievedBatch(std::streamsize size_to_write);

    void ValidateAndWrite(
        int block_index,
        const char *buffer,
        std::streamsize count);

    std::streamsize Decompress(
        std::streamsize compressed_size,
        const void *decompression_buffer,
        void *output_buffer) const;

  public:
    ChunkReconstructor(
        SyncCommand &parent,
        std::streamoff start_offset,
        const FileStream &output_file_stream);

    void ReconstructFromSeed(int block_index, std::streamoff seed_offset);
    void EnqueueBlockRetrieval(int block_index, std::streamoff begin_offset);
    void FlushBatch(bool force);
  };

public:
  explicit SyncCommand(
      const std::string &data_uri,
      const std::string &metadata_uri,
      std::string seed_uri,
      std::filesystem::path output_path,
      bool compression_disabled,
      int num_blocks_in_batch,
      int threads);

  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_SYNC_COMMAND_H
