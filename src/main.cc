#include <gflags/gflags.h>
#include <glog/logging.h>

#include <fstream>

#include "commands/prepare_command.h"
#include "commands/sync_command.h"
#include "monitor.h"

DEFINE_string(command, "", "prepare, sync, ...");  // NOLINT
DEFINE_string(input_filename, "", "input file");   // NOLINT
// TODO(kyotov): maybe output_path? it is used elsewhere...
DEFINE_string(  // NOLINT
    output_kysync_filename,
    "",
    "output kysync file");
DEFINE_string(  // NOLINT
    output_compressed_filename,
    "",
    "output compressed file");
DEFINE_string(output_filename, "", "output file");                  // NOLINT
DEFINE_string(data_uri, "", "data uri");                            // NOLINT
DEFINE_string(metadata_uri, "", "metadata uri");                    // NOLINT
DEFINE_string(seed_data_uri, "", "seed data uri");                  // NOLINT
DEFINE_uint32(block_size, 1024, "block size");                      // NOLINT
DEFINE_int32(threads, 32, "number of threads");                     // NOLINT
DEFINE_int32(num_blocks_in_batch, 4, "number of blocks in batch");  // NOLINT
DEFINE_bool(use_compression, true, "use compression");              // NOLINT

int main(int argc, char **argv) {
  try {
    try {
      google::InitGoogleLogging(argv[0]);
      gflags::ParseCommandLineFlags(&argc, &argv, true);

      FLAGS_logtostderr = true;
      FLAGS_colorlogtostderr = true;

      LOG(INFO) << "ksync v0.1";

      if (FLAGS_command == "prepare") {
        if (FLAGS_output_kysync_filename.empty()) {
          FLAGS_output_kysync_filename = FLAGS_input_filename + ".kysync";
          LOG(INFO) << "metadata filename defaulted to " << FLAGS_output_kysync_filename;
        }
        if (FLAGS_output_compressed_filename.empty()) {
          FLAGS_output_compressed_filename = FLAGS_input_filename + ".pzst";
          LOG(INFO) << "compressed output defaulted to " << FLAGS_output_compressed_filename;
        }

        auto command = kysync::PrepareCommand(
            FLAGS_input_filename,
            FLAGS_output_kysync_filename,
            FLAGS_output_compressed_filename,
            FLAGS_block_size,
            FLAGS_threads);

        return kysync::Monitor(command).Run();
      }

      if (FLAGS_command == "sync") {
        auto output = std::ofstream(FLAGS_output_filename, std::ios::binary);
        CHECK(output) << "unable to write to " << FLAGS_output_filename;

        auto command = kysync::SyncCommand(
            FLAGS_data_uri,
            FLAGS_metadata_uri,
            !FLAGS_seed_data_uri.empty() ? FLAGS_seed_data_uri
                                         : "file://" + FLAGS_input_filename,
            FLAGS_output_filename,
            !FLAGS_use_compression,
            FLAGS_num_blocks_in_batch,
            FLAGS_threads);

        return kysync::Monitor(command).Run();
      }

      CHECK(false) << "unhandled command";
      return 1;
    } catch (const std::exception &e) {
      LOG(ERROR) << e.what();
      return 2;
    } catch (...) {
      LOG(ERROR) << "unknown exception";
    }
  } catch (...) {
    // silence bugprone-exception-escape
  }
}
