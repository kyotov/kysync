#include <glog/logging.h>
#include <ky/process.h>
#include <sys/prctl.h>  // prctl(), PR_SET_PDEATHSIG
#include <unistd.h>     // fork()

#include <csignal>
#include <vector>

namespace ky {

struct ProcessWatcher::State {};

/***
 * Posix Version.
 * https://stackoverflow.com/questions/284325/how-to-make-child-process-die-after-parent-exits
 */
ProcessWatcher::ProcessWatcher() : state_(std::make_unique<State>()) {}

ProcessWatcher::~ProcessWatcher() = default;

// NOTE: this cannot be static on Windows, and the interface is shared!
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ProcessWatcher::Execute(const std::vector<std::string> &argv) {
  pid_t ppid_before_fork = getpid();
  pid_t pid = fork();
  CHECK_NE(pid, -1);

  if (pid == 0) {
    // we need to call prctl... :) no way around it if we follow this recipe.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
    CHECK_NE(r, -1);

    // if original parent exited before prctl() call...
    if (getppid() != ppid_before_fork) {
      exit(1); // NOLINT(concurrency-mt-unsafe)
    }

    // execute the process
    auto v = std::vector<char *>(argv.size() + 1);
    for (int i = 0; i < argv.size(); i++) {
      // NOTE: not sure why the C interface does not take const char *
      //       ... but I sure hope it does not modify the data
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      v[i] = const_cast<char *>(argv[i].c_str());
    }
    v[argv.size()] = nullptr;

    auto e = std::vector<char *>();
    e.push_back(nullptr);

    execve(v[0], v.data(), e.data());
  }
}

}  // namespace ky