#include <glog/logging.h>
#include <ky/min.h>
#include <ky/parallelize.h>
#include <kysync/streams.h>
#include <kysync/test_commands/gen_data_command.h>

#include <filesystem>
#include <fstream>
#include <random>

namespace kysync {

namespace fs = std::filesystem;

using RandomEngine = std::default_random_engine;
using RandomValueType = RandomEngine::result_type;

GenDataCommand::GenDataCommand(
    const fs::path &output_path,
    std::streamsize data_size,
    std::streamsize seed_data_size,
    std::streamsize fragment_size,
    int similarity)
    : Observable("gen_data"),
      path_(output_path),
      data_file_path_(output_path / "data.bin"),
      seed_data_file_path_(output_path / "seed_data.bin"),
      data_size_(data_size),
      seed_data_size_(seed_data_size != -1 ? seed_data_size : data_size),
      fragment_size_(fragment_size),
      diff_size_(fragment_size * (100 - similarity) / 100)  //
{
  CHECK(fs::is_directory(path_)) << path_;
  CHECK_LE(similarity, 100);

  // TODO(kyotov): can we automate this for all fields?
  LOG(INFO) << "data: " << data_file_path_;
  LOG(INFO) << "data size: " << data_size_;
  LOG(INFO) << "seed data: " << seed_data_file_path_;
  LOG(INFO) << "seed data size: " << seed_data_size_;

  LOG(INFO) << "RandomEngine: " << typeid(RandomEngine).name();
  LOG(INFO) << "sizeof(RandomValueType): " << sizeof(RandomValueType);
};

static std::vector<RandomValueType> GenVec(
    std::streamsize size,
    RandomEngine &random_engine) {
  auto result = std::vector<RandomValueType>();
  for (int i = 0; i < size; i += sizeof(RandomValueType)) {
    result.push_back(random_engine());
  }
  return result;
}

void GenDataCommand::GenChunk(int id, std::streamoff beg, std::streamoff end) {
  auto mode = std::ios::binary | std::ios::in | std::ios::out;

  std::fstream data_stream(data_file_path_, mode);
  data_stream.seekp(beg);

  std::fstream seed_data_stream(seed_data_file_path_, mode);
  seed_data_stream.seekp(beg);

  auto random = RandomEngine(id);
  auto random_value_size = static_cast<int>(sizeof(RandomValueType));

  for (std::streamoff i = beg; i < end; i += fragment_size_) {
    auto diff_offset = static_cast<std::streamoff>(
        random() % (fragment_size_ - diff_size_ + 1));

    auto v_data = GenVec(fragment_size_, random);

    std::streamsize s =
        StreamWrite(data_stream, v_data, ky::Min(data_size_, end) - i);
    AdvanceProgress(s);

    auto v_diff = GenVec(diff_size_, random);
    std::copy(
        v_diff.begin(),
        v_diff.end(),
        v_data.begin() + diff_offset / random_value_size);
    s = StreamWrite(
        seed_data_stream,
        v_data,
        ky::Min(seed_data_size_, end) - i);
    AdvanceProgress(s);
  }
}

static void CreateFile(const fs::path &path, std::streamsize size) {
  std::ofstream(path, std::ios::binary).close();
  std::error_code ec{};
  fs::resize_file(path, size, ec);
  CHECK(ec.value() == 0) << ec.message();
}

int GenDataCommand::Run() {
  CHECK_EQ(data_size_, seed_data_size_)
      << "different data and seed data sizes not supported yet";

  CreateFile(data_file_path_, data_size_);
  CreateFile(seed_data_file_path_, seed_data_size_);

  StartNextPhase(data_size_ + seed_data_size_);

  ky::parallelize::Parallelize(
      data_size_,
      fragment_size_,
      0,
      // FIXME(kyotov): this should not be hardcoded!
      32,
      [this](auto id, auto beg, auto end) { GenChunk(id, beg, end); });

  StartNextPhase(0);

  return 0;
}

}  // namespace kysync
