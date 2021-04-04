#include <gflags/gflags.h>
#include <glog/logging.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

DEFINE_string(output_path, ".", "output directory");          // NOLINT
DEFINE_int64(data_size, 1'000'000'000, "default data size");  // NOLINT
DEFINE_int64(seed_data_size, -1, "default seed data size");   // NOLINT
DEFINE_int64(fragment_size, 123'456, "fragment size");        // NOLINT
DEFINE_int64(similarity, 90, "percentage similarity");        // NOLINT

namespace fs = std::filesystem;

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;

  auto path = fs::path(FLAGS_output_path);
  CHECK(fs::is_directory(path));

  std::ofstream data(path / "data.bin", std::ios::binary);
  std::ofstream seed_data(path / "seed_data.bin", std::ios::binary);

  auto difference = 100 - FLAGS_similarity;
  CHECK(0 <= difference && difference <= 100);

  auto random = std::default_random_engine();
  random.seed(1);

  using U = std::default_random_engine::result_type;
  std::vector<U> t;

  for (auto curr = 0ULL;                                        //
       curr < std::max(FLAGS_data_size, FLAGS_seed_data_size);  //
       curr += FLAGS_fragment_size)
  {
    LOG(INFO) << curr;

    auto diff_size = FLAGS_fragment_size * difference / 100;
    auto diff_offset = random() % (FLAGS_fragment_size - diff_size + 1);

    t.clear();
    for (auto i = 0; i < FLAGS_fragment_size; i += sizeof(U)) {
      t.push_back(random());
    }
    data.write(reinterpret_cast<const char *>(t.data()), FLAGS_fragment_size);

    for (auto i = diff_offset; i < diff_offset + diff_size; i += sizeof(U)) {
      t[i / sizeof(U)] = random();
    }
    seed_data.write(
        reinterpret_cast<const char *>(t.data()),
        FLAGS_fragment_size);
  }
}
