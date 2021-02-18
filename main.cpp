#include <gflags/gflags.h>
#include <glog/logging.h>

#include <fstream>

#include "Monitor.h"
#include "commands/PrepareCommand.h"
#include "commands/SyncCommand.h"

namespace fs = std::filesystem;

DEFINE_string(command, "", "prepare, sync, ...");  // NOLINT(cert-err58-cpp)
DEFINE_string(input_filename, "", "input file");   // NOLINT(cert-err58-cpp)
// TODO: maybe output_path? it is used elsewhere...
DEFINE_string(output_filename, "", "output file");  // NOLINT(cert-err58-cpp)
DEFINE_string(data_uri, "", "data uri");            // NOLINT(cert-err58-cpp)
DEFINE_string(metadata_uri, "", "data uri");        // NOLINT(cert-err58-cpp)
DEFINE_uint32(block, 1024, "block size");           // NOLINT(cert-err58-cpp)
DEFINE_uint32(threads, 32, "number of threads");    // NOLINT(cert-err58-cpp)

int main(int argc, char **argv)
{
  try {
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "ksync v0.1";

    if (FLAGS_command == "prepare") {
      if (FLAGS_output_filename.empty()) {
        FLAGS_output_filename = FLAGS_input_filename + ".ksync";
      }

      auto input = std::ifstream(FLAGS_input_filename, std::ios::binary);
      auto output = std::ofstream(FLAGS_output_filename, std::ios::binary);
      CHECK(output) << "unable to write to " << FLAGS_output_filename;

      auto c = PrepareCommand(input, output, FLAGS_block);
      return Monitor(c).run();
    }

    if (FLAGS_command == "sync") {
      if (FLAGS_metadata_uri.empty()) {
        FLAGS_metadata_uri = FLAGS_data_uri + ".ksync";
      }

      auto output = std::ofstream(FLAGS_output_filename, std::ios::binary);
      CHECK(output) << "unable to write to " << FLAGS_output_filename;

      auto c = SyncCommand(
          FLAGS_data_uri,
          FLAGS_metadata_uri,
          "file://" + FLAGS_input_filename,
          FLAGS_output_filename,
          FLAGS_threads);
      return Monitor(c).run();
    }

    CHECK(false) << "unhandled command";
    return 1;
  }
  catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return 2;
  }
}
