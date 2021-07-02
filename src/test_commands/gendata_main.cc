#include <gflags/gflags.h>
#include <glog/logging.h>
#include <ky/noexcept.h>
#include <ky/observability/observer.h>
#include <kysync/test_commands/gen_data_command.h>

#include <vector>

DEFINE_string(command, "", "command to execute");             // NOLINT
DEFINE_string(output_path, ".", "output directory");          // NOLINT
DEFINE_int64(data_size, 1'000'000'000, "default data size");  // NOLINT
DEFINE_int64(seed_data_size, -1, "default seed data size");   // NOLINT
DEFINE_int64(fragment_size, 123'456, "fragment size");        // NOLINT
DEFINE_int32(similarity, 90, "percentage similarity");        // NOLINT

int main(int argc, char **argv) {
  ky::NoExcept([&argc, &argv]() {
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
      return ky::observability::Observer(c).Run([&c]() { return c.Run(); });
    }

    CHECK(false) << "unhandled command";
    return 1;
  });
}
