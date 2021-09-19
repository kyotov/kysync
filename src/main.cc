#include <gflags/gflags.h>
#include <glog/logging.h>
#include <ky/noexcept.h>
#include <ky/observability/observer.h>
#include <kysync/commands/prepare_command.h>
#include <kysync/commands/sync_command.h>

#include <fstream>

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

DECLARE_bool(help);      // NOLINT
DECLARE_string(helpon);  // NOLINT

int main(int argc, char **argv) {
  ky::NoExcept([&argc, &argv]() {
    google::InitGoogleLogging(argv[0]);

    gflags::SetUsageMessage("--command=[prepare|sync] ...");
    gflags::SetVersionString("v0.1");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    if (FLAGS_command == "prepare") {
      if (FLAGS_output_kysync_filename.empty()) {
        FLAGS_output_kysync_filename = FLAGS_input_filename + ".kysync";
        LOG(INFO) << "metadata filename defaulted to "
                  << FLAGS_output_kysync_filename;
      }
      
      if (FLAGS_output_compressed_filename.empty()) {
        FLAGS_output_compressed_filename = FLAGS_input_filename + ".pzst";
        LOG(INFO) << "compressed output defaulted to "
                  << FLAGS_output_compressed_filename;
      }

      auto c = kysync::PrepareCommand::Create(
          FLAGS_input_filename,
          FLAGS_output_kysync_filename,
          FLAGS_output_compressed_filename,
          FLAGS_block_size,
          FLAGS_threads);

      return ky::observability::Observer(*c).Run([&c]() { return c->Run(); });
    }

    if (FLAGS_command == "sync") {
      auto output = std::ofstream(FLAGS_output_filename, std::ios::binary);
      CHECK(output) << "unable to write to " << FLAGS_output_filename;

      if (FLAGS_metadata_uri.empty()) {
        FLAGS_metadata_uri = FLAGS_data_uri + ".kysync";
        LOG(INFO) << "metadata uri defaulted to " << FLAGS_metadata_uri;
      }

      if (FLAGS_use_compression && !FLAGS_data_uri.ends_with(".pzst")) {
        FLAGS_data_uri += ".pzst";
        LOG(INFO) << "data uri defaulted to " << FLAGS_data_uri;
      }

      if (FLAGS_seed_data_uri.empty()) {
        FLAGS_seed_data_uri = "file://" + FLAGS_input_filename;
        LOG(INFO) << "seed data uri defaulted to " << FLAGS_seed_data_uri;
      }

      auto c = kysync::SyncCommand::Create(
          FLAGS_data_uri,
          FLAGS_metadata_uri,
          FLAGS_seed_data_uri,
          FLAGS_output_filename,
          !FLAGS_use_compression,
          FLAGS_num_blocks_in_batch,
          FLAGS_threads);

      return ky::observability::Observer(*c).Run([&c]() { return c->Run(); });
    }

    LOG(ERROR) << "expected `--command=prepare` or `--command=sync`";

    FLAGS_help = false;
    FLAGS_helpon = "main";
    gflags::HandleCommandLineHelpFlags();

    return 1;
  });
}
