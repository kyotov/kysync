#include "GenDataCommand.h"

#include <glog/logging.h>

#include <filesystem>
#include <fstream>
#include <random>

#include "../../utilities/parallelize.h"
#include "../../utilities/streams.h"

namespace kysync {

namespace fs = std::filesystem;

struct GenDataCommand::Impl {
  const GenDataCommand &kParent;
  const fs::path kPath;
  const fs::path kData;
  const fs::path kSeedData;
  const size_t kDataSize;
  const size_t kSeedDataSize;
  const size_t kFragmentSize;
  const uint32_t kSimilarity;
  const size_t kDiffSize;

  void GenChunk(int id, size_t beg, size_t end);
};

GenDataCommand::GenDataCommand(
    const fs::path &output_path,
    uint64_t data_size,
    uint64_t seed_data_size,
    uint64_t fragment_size,
    uint32_t similarity)
    : impl_(new Impl{
          .kParent = *this,
          .kPath = output_path,
          .kData = output_path / "data.bin",
          .kSeedData = output_path / "seed_data.bin",
          .kDataSize = data_size,
          .kSeedDataSize = seed_data_size != -1ULL ? seed_data_size : data_size,
          .kFragmentSize = fragment_size,
          .kSimilarity = similarity,
          .kDiffSize = fragment_size * (100 - similarity) / 100}) {
  CHECK(fs::is_directory(impl_->kPath));
  CHECK_LE(similarity, 100);

  // TODO: can we automate this for all fields?
  LOG(INFO) << "data: " << impl_->kData;
  LOG(INFO) << "data size: " << impl_->kDataSize;
  LOG(INFO) << "seed data: " << impl_->kSeedData;
  LOG(INFO) << "seed data size: " << impl_->kSeedDataSize;
};

GenDataCommand::~GenDataCommand() noexcept = default;

using R = std::default_random_engine;
using U = R::result_type;

std::vector<U> GenVec(size_t size, R &random) {
  auto result = std::vector<U>();
  for (size_t i = 0; i < size; i += sizeof(U)) {
    result.push_back(random());
  }
  return result;
}

void GenDataCommand::Impl::GenChunk(int id, size_t beg, size_t end) {
  auto mode = std::ios::binary | std::ios::in | std::ios::out;

  std::fstream data_stream(kData, mode);
  data_stream.seekp(beg);

  std::fstream seed_data_stream(kSeedData, mode);
  seed_data_stream.seekp(beg);

  auto random = R(id);

  for (size_t i = beg; i < end; i += kFragmentSize) {
    auto diff_offset = random() % (kFragmentSize - kDiffSize + 1);

    auto v_data = GenVec(kFragmentSize, random);

    size_t s = StreamWrite(data_stream, v_data, std::min(kDataSize, end) - i);
    kParent.AdvanceProgress(s);

    auto v_diff = GenVec(kDiffSize, random);
    std::copy(
        v_diff.begin(),
        v_diff.end(),
        v_data.begin() + diff_offset / sizeof(U));
    s = StreamWrite(seed_data_stream, v_data, std::min(kSeedDataSize, end) - i);
    kParent.AdvanceProgress(s);
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
  CHECK_EQ(impl_->kDataSize, impl_->kSeedDataSize)
      << "different data and seed data sizes not supported yet";

  CreateFile(impl_->kData, impl_->kDataSize);
  CreateFile(impl_->kSeedData, impl_->kSeedDataSize);

  StartNextPhase(impl_->kDataSize + impl_->kSeedDataSize);

  Parallelize(
      impl_->kDataSize,
      impl_->kFragmentSize,
      0,
      32,
      [this](auto id, auto beg, auto end) { impl_->GenChunk(id, beg, end); });

  StartNextPhase(0);

  return 0;
}

}  // namespace kysync
