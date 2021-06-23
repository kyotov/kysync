#include <gflags/gflags.h>
#include <glog/logging.h>
#include <ky/noexcept.h>
#include <ky/process.h>
#include <ky/timer.h>
#include <kysync/test_locations.h>

#include "test_file_chatter.h"

DEFINE_string(name, "", "process name");                // NOLINT
DEFINE_string(input_path, "", "input channel path");    // NOLINT
DEFINE_string(output_path, "", "output channel path");  // NOLINT
DEFINE_bool(spawn_a_child, false, "spawn a child");     // NOLINT

int main(int argc, char **argv) {
  ky::NoExcept([&argc, &argv]() {
    using namespace std::chrono_literals;

    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    CHECK(!FLAGS_name.empty());
    CHECK(!FLAGS_input_path.empty());
    CHECK(!FLAGS_output_path.empty());

    LOG(INFO) << "input channel: " << FLAGS_input_path;
    LOG(INFO) << "output channel: " << FLAGS_output_path;

    auto chat =
        ky::TestFileChatter(FLAGS_name, FLAGS_input_path, FLAGS_output_path);

    auto pw = ky::ProcessWatcher();

    if (FLAGS_spawn_a_child) {
      LOG(INFO) << "spawining a child...";
      auto root_path = std::filesystem::path(FLAGS_input_path).parent_path();
      pw.Execute(
          {kTestChatBotExecutable.string(),
           "--name",
           "child",
           "--input-path",
           (root_path / "child_input").string(),
           "--output-path",
           (root_path / "child_output").string()});
    }

    auto beg = ky::timer::Now();

    while (ky::timer::DeltaMs(beg, ky::timer::Now()) < 5000) {
      auto line = chat.Read();
      if (line == "stop") {
        LOG(INFO) << "exiting...";
        break;
      }
      if (line == "ping") {
        chat.Write("pong");
      }
    }

    return 0;
  });
}
