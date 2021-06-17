#ifndef KSYNC_SRC_KY_OBSERVABILITY_INCLUDE_KY_OBSERVABILITY_OBSERVER_H
#define KSYNC_SRC_KY_OBSERVABILITY_INCLUDE_KY_OBSERVABILITY_OBSERVER_H

#include <ky/observability/observable.h>
#include <ky/timer.h>

#include <functional>
#include <list>
#include <unordered_map>

namespace ky::observability {

class Observer final {
  Observable &observable_;

  ky::timer::Timestamp ts_total_begin_;
  ky::timer::Timestamp ts_phase_begin_;

  struct Phase {
    Observable::ValueType bytes{};
    Observable::ValueType ms{};
  };

  std::list<Phase> phases_;

  void Update();

public:
  explicit Observer(Observable &observable);

  int Run(const std::function<int(void)> &task);

  void SnapshotPhases(const std::function<void(
                          const std::string &key,
                          const Observable::ValueType &value)> &callback);
};

}  // namespace ky::observability

#endif  // KSYNC_SRC_KY_OBSERVABILITY_INCLUDE_KY_OBSERVABILITY_OBSERVER_H
