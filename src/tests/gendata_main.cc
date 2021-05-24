#include <gflags/gflags.h>
#include <glog/logging.h>

#include <vector>

#include "../monitor.h"
#include "commands/gen_data_command.h"

DEFINE_string(command, "", "command to execute");                // NOLINT
DEFINE_string(output_path, ".", "output directory");             // NOLINT
DEFINE_uint64(data_size, 1'000'000'000, "default data size");    // NOLINT
DEFINE_uint64(seed_data_size, -1ULL, "default seed data size");  // NOLINT
DEFINE_uint64(fragment_size, 123'456, "fragment size");          // NOLINT
DEFINE_uint32(similarity, 90, "percentage similarity");          // NOLINT

int main(int argc, char **argv) {
  try {
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    if (FLAGS_command == "gendata") {
      auto c = kysync::GenDataCommand(
          FLAGS_output_path,
          FLAGS_data_size,
          FLAGS_seed_data_size,
          FLAGS_fragment_size,
          FLAGS_similarity);
      return kysync::Monitor(c).Run();
    }

    if (FLAGS_command == "measure") {
    }

    CHECK(false) << "unhandled command";
    return 1;
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return 2;
  }
}
