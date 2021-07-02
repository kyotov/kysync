#ifndef KSYNC_SRC_KY_PROCESS_INCLUDE_KY_PROCESS_H
#define KSYNC_SRC_KY_PROCESS_INCLUDE_KY_PROCESS_H

#include <memory>
#include <string>

namespace ky {

class ProcessWatcher {
public:
  ProcessWatcher();
  ~ProcessWatcher();

  void Execute(const std::vector<std::string> &argv);

private:
  class State;
  std::unique_ptr<State> state_;
};

}  // namespace ky

#endif  // KSYNC_SRC_KY_PROCESS_INCLUDE_KY_PROCESS_H
