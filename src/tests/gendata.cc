#include <gflags/gflags.h>
#include <glog/logging.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "../Monitor.h"
#include "../commands/impl/CommandImpl.h"
#include "../utilities/parallelize.h"

DEFINE_string(output_path, ".", "output directory");          // NOLINT
DEFINE_int64(data_size, 1'000'000'000, "default data size");  // NOLINT
DEFINE_int64(seed_data_size, -1, "default seed data size");   // NOLINT
DEFINE_int64(fragment_size, 123'456, "fragment size");        // NOLINT
DEFINE_int64(similarity, 90, "percentage similarity");        // NOLINT

namespace fs = std::filesystem;

namespace kysync {

class Gen : public Command {
  const fs::path kPath;
  const fs::path kData;
  const fs::path kSeedData;
  const uint64_t kDiffSize =
      FLAGS_fragment_size * (100 - FLAGS_similarity) / 100;

  using R = std::default_random_engine;
  using U = R::result_type;

public:
  Gen()
      : kPath(FLAGS_output_path),
        kData(kPath / "data.bin"),
        kSeedData(kPath / "seed_data.bin") {
    LOG(INFO) << "data: " << kData;
    LOG(INFO) << "seed data: " << kSeedData;
    CHECK(fs::is_directory(kPath));
  }

  explicit Gen(Command &&c) : Command(std::move(c)) {}

  static std::vector<U> GenVec(size_t size, R &random) {
    auto result = std::vector<U>();
    for (size_t i = 0; i < size; i += sizeof(U)) {
      result.push_back(random());
    }
    return result;
  }

  void GenChunk(int id, size_t beg, size_t end) {
    auto mode = std::ios::binary | std::ios::in | std::ios::out;

    std::fstream data(kData, mode);
    data.seekp(beg);

    std::fstream seed_data(kSeedData, mode);
    seed_data.seekp(beg);

    auto random = R(id);

    for (size_t i = beg; i < end; i += FLAGS_fragment_size) {
      auto diff_offset = random() % (FLAGS_fragment_size - kDiffSize + 1);

      auto v_data = GenVec(FLAGS_fragment_size, random);
      data.write(
          reinterpret_cast<const char *>(v_data.data()),
          FLAGS_fragment_size);
      impl_->progress_current_bytes_ += FLAGS_fragment_size;

      auto v_diff = GenVec(kDiffSize, random);
      std::copy(
          v_diff.begin(),
          v_diff.end(),
          v_data.begin() + diff_offset / sizeof(U));
      seed_data.write(
          reinterpret_cast<const char *>(v_data.data()),
          FLAGS_fragment_size);
      impl_->progress_current_bytes_ += FLAGS_fragment_size;
    }
  }

  int Run() override {
    // TODO: fix these to be different
    FLAGS_seed_data_size = FLAGS_data_size;

    LOG(INFO) << "start";

    std::error_code ec;

    std::ofstream data(kData, std::ios::binary);
    ec.clear();
    fs::resize_file(kData, FLAGS_data_size, ec);
    CHECK(ec.value() == 0) << ec.message();

    std::ofstream seed_data(kSeedData, std::ios::binary);
    ec.clear();
    fs::resize_file(kSeedData, FLAGS_seed_data_size, ec);
    CHECK(ec.value() == 0) << ec.message();

    impl_->progress_phase_++;
    impl_->progress_total_bytes_ = FLAGS_data_size + FLAGS_seed_data_size;
    impl_->progress_current_bytes_ = 0;

    LOG(INFO) << "data size is " << FLAGS_data_size;

    Parallelize(
        FLAGS_data_size,
        FLAGS_fragment_size,
        0,
        32,
        [this](auto id, auto beg, auto end) { GenChunk(id, beg, end); });

    return 0;
  }
};

}  // namespace kysync

int main(int argc, char **argv) {
  using namespace kysync;

  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;

  auto c = Gen();
  return Monitor(c).Run();
}
