#include <glog/logging.h>
#include <ky/process.h>
#include <signal.h>     // signals
#include <stdio.h>      // perror()
#include <unistd.h>     // fork()
#include <sys/wait.h>

#include <vector>

namespace ky {

struct ProcessWatcher::State {};

ProcessWatcher::ProcessWatcher() : state_(std::make_unique<State>()) {}

ProcessWatcher::~ProcessWatcher() = default;

void ProcessWatcher::Execute(const std::vector<std::string> &argv) {
  pid_t pid_parent = fork();

  if (pid_parent != 0) {
    pid_t pid_child = fork();
    if (pid_child != 0) {
      // watchdog
      LOG(INFO) << "watchdog is " << getpid();
      LOG(INFO) << "parent is " << pid_parent;
      LOG(INFO) << "child is " << pid_child;

      // what if the parent is dead here?

      LOG(INFO) << "watchdog waiting for parent " << pid_parent << " to die...";
      pid_t pid_done = waitpid(pid_parent, nullptr, 0);

      if (pid_done == pid_parent) {
        LOG(INFO) << "watchdog killing child << " << pid_child << "...";
        kill(pid_child, SIGKILL);
      }

      LOG(INFO) << "watchdog exiting...";
      exit(0);

    } else {
      // child

      auto v = std::vector<char *>(argv.size() + 1);
      for (int i = 0; i < argv.size(); i++) {
        v[i] = const_cast<char *>(argv[i].c_str());
      }
      v[argv.size()] = nullptr;

      auto e = std::vector<char *>();
      e.push_back(nullptr);

      execve(v[0], v.data(), e.data());

      CHECK(false);
    }
  }
  // original parent
}

}  // namespace ky