#include "gen_data_command.h"

#include <glog/logging.h>

#include <filesystem>
#include <fstream>
#include <random>

#include "../../utilities/parallelize.h"
#include "../../utilities/streams.h"

namespace kysync {

namespace fs = std::filesystem;

using RandomEngine = std::default_random_engine;
using RandomValueType = RandomEngine::result_type;

GenDataCommand::GenDataCommand(
    const fs::path &output_path,
    uint64_t data_size,
    uint64_t seed_data_size,
    uint64_t fragment_size,
    uint32_t similarity)
    : kPath(output_path),
      kData(output_path / "data.bin"),
      kSeedData(output_path / "seed_data.bin"),
      kDataSize(data_size),
      kSeedDataSize(seed_data_size != -1ULL ? seed_data_size : data_size),
      kFragmentSize(fragment_size),
      kDiffSize(fragment_size * (100 - similarity) / 100)  //
{
  CHECK(fs::is_directory(kPath));
  CHECK_LE(similarity, 100);

  // TODO: can we automate this for all fields?
  LOG(INFO) << "data: " << kData;
  LOG(INFO) << "data size: " << kDataSize;
  LOG(INFO) << "seed data: " << kSeedData;
  LOG(INFO) << "seed data size: " << kSeedDataSize;

  LOG(INFO) << "RandomEngine: " << typeid(RandomEngine).name();
  LOG(INFO) << "sizeof(RandomValueType): " << sizeof(RandomValueType);
};

std::vector<RandomValueType> GenVec(size_t size, RandomEngine &random_engine) {
  auto result = std::vector<RandomValueType>();
  for (size_t i = 0; i < size; i += sizeof(RandomValueType)) {
    result.push_back(random_engine());
  }
  return result;
}

void GenDataCommand::GenChunk(int id, size_t beg, size_t end) {
  auto mode = std::ios::binary | std::ios::in | std::ios::out;

  std::fstream data_stream(kData, mode);
  data_stream.seekp(beg);

  std::fstream seed_data_stream(kSeedData, mode);
  seed_data_stream.seekp(beg);

  auto random = RandomEngine(id);

  for (size_t i = beg; i < end; i += kFragmentSize) {
    auto diff_offset = random() % (kFragmentSize - kDiffSize + 1);

    auto v_data = GenVec(kFragmentSize, random);

    size_t s = StreamWrite(data_stream, v_data, std::min(kDataSize, end) - i);
    AdvanceProgress(s);

    auto v_diff = GenVec(kDiffSize, random);
    std::copy(
        v_diff.begin(),
        v_diff.end(),
        v_data.begin() + diff_offset / sizeof(RandomValueType));
    s = StreamWrite(seed_data_stream, v_data, std::min(kSeedDataSize, end) - i);
    AdvanceProgress(s);
  }
}

void CreateFile(const fs::path &path, size_t size) {
  {
    std::ofstream(path, std::ios::binary);
  }
  std::error_code ec;
  ec.clear();
  fs::resize_file(path, size, ec);
  CHECK(ec.value() == 0) << ec.message();
}

int GenDataCommand::Run() {
  CHECK_EQ(kDataSize, kSeedDataSize)
      << "different data and seed data sizes not supported yet";

  CreateFile(kData, kDataSize);
  CreateFile(kSeedData, kSeedDataSize);

  StartNextPhase(kDataSize + kSeedDataSize);

  Parallelize(
      kDataSize,
      kFragmentSize,
      0,
      32,
      [this](auto id, auto beg, auto end) { GenChunk(id, beg, end); });

  StartNextPhase(0);

  return 0;
}

}  // namespace kysync
