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
DEFINE_string(metadata_uri, "", "metadata uri");    // NOLINT
DEFINE_string(seed_data_uri, "", "seed data uri");  // NOLINT
DEFINE_uint32(block_size, 1024, "block size");      // NOLINT
DEFINE_uint32(threads, 32, "number of threads");    // NOLINT
DEFINE_bool(compression, true, "use compression");  // NOLINT

}  // namespace kysync

int main(int argc, char **argv) {
  using namespace kysync;

  try {
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "ksync v0.1";

    if (FLAGS_command == "prepare") {
      auto command = PrepareCommand::Create(
          FLAGS_input_filename,
          FLAGS_output_ksync_filename,
          FLAGS_output_compressed_filename,
          FLAGS_block_size);

      return Monitor(command).Run();
    }

    if (FLAGS_command == "sync") {
      auto output = std::ofstream(FLAGS_output_filename, std::ios::binary);
      CHECK(output) << "unable to write to " << FLAGS_output_filename;

      auto command = SyncCommand(
          FLAGS_data_uri,
          FLAGS_metadata_uri,
          !FLAGS_seed_data_uri.empty() ? FLAGS_seed_data_uri
                                       : "file://" + FLAGS_input_filename,
          FLAGS_output_filename,
          !FLAGS_compression,
          FLAGS_threads);

      return Monitor(command).Run();
    }

    CHECK(false) << "unhandled command";
    return 1;
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return 2;
  }
}
