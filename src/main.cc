#include <gflags/gflags.h>
#include <glog/logging.h>

#include <fstream>

#include "Monitor.h"
#include "commands/PrepareCommand.h"
#include "commands/SyncCommand.h"

namespace kysync {

DEFINE_string(command, "", "prepare, sync, ...");  // NOLINT
DEFINE_string(input_filename, "", "input file");   // NOLINT
// TODO: maybe output_path? it is used elsewhere...
DEFINE_string(  // NOLINT
    output_ksync_filename,
    "",
    "output ksync file");
DEFINE_string(  // NOLINT
    output_compressed_filename,
    "",
    "output compressed file");
DEFINE_string(output_filename, "", "output file");  // NOLINT
DEFINE_string(data_uri, "", "data uri");            // NOLINT
DEFINE_string(metadata_uri, "", "data uri");        // NOLINT
DEFINE_uint32(block, 1024, "block size");           // NOLINT
DEFINE_uint32(threads, 32, "number of threads");    // NOLINT
DEFINE_bool(                                        // NOLINT
    compression_disabled,
    false,
    "true implies source file does not have compression");

}  // namespace kysync

int main(int argc, char **argv) {
  using namespace kysync;

  try {
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "ksync v0.1";

    if (FLAGS_command == "Prepare") {
      if (FLAGS_output_ksync_filename.empty()) {
        FLAGS_output_ksync_filename = FLAGS_input_filename + ".ksync";
      }
      auto output_ksync =
          std::ofstream(FLAGS_output_ksync_filename, std::ios::binary);
      CHECK(output_ksync) << "unable to write to "
                          << FLAGS_output_ksync_filename;

      if (FLAGS_output_compressed_filename.empty()) {
        FLAGS_output_compressed_filename = FLAGS_input_filename + ".pzst";
      }
      auto output_compressed =
          std::ofstream(FLAGS_output_compressed_filename, std::ios::binary);
      CHECK(output_compressed)
          << "unable to write to " << FLAGS_output_compressed_filename;

      auto input = std::ifstream(FLAGS_input_filename, std::ios::binary);
      auto c =
          PrepareCommand(input, output_ksync, output_compressed, FLAGS_block);
      return Monitor(c).Run();
    }

    if (FLAGS_command == "sync") {
      if (FLAGS_metadata_uri.empty()) {
        FLAGS_metadata_uri = FLAGS_data_uri + ".ksync";
      }

      auto output = std::ofstream(FLAGS_output_filename, std::ios::binary);
      CHECK(output) << "unable to write to " << FLAGS_output_filename;

      auto c = SyncCommand(
          FLAGS_data_uri + (FLAGS_compression_disabled ? "" : ".pzst"),
          FLAGS_compression_disabled,
          FLAGS_metadata_uri,
          "file://" + FLAGS_input_filename,
          FLAGS_output_filename,
          FLAGS_threads);
      return Monitor(c).Run();
    }

    CHECK(false) << "unhandled command";
    return 1;
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return 2;
  }
}
