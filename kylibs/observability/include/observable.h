#ifndef KSYNC_SRC_KY_OBSERVABILITY_INCLUDE_KY_OBSERVABILITY_OBSERVABLE_H
#define KSYNC_SRC_KY_OBSERVABILITY_INCLUDE_KY_OBSERVABILITY_OBSERVABLE_H

#include <atomic>
#include <iostream>
#include <string>

namespace ky::observability {

class Observable {
public:
  using ValueType = std::streamsize;

  explicit Observable(std::string name);

  const std::string &GetName() const;

  int GetPhase() const;
  void AdvancePhase();

  bool IsReadyForNextPhase() const;

  ValueType GetTotal() const;

  ValueType GetProgress() const;
  void AdvanceProgress(ValueType delta);

  void EnableMonitor();
  void StartNextPhase(ValueType total);

private:
  std::string name_;

  bool is_monitored_;

  std::atomic<bool> is_ready_for_next_phase_;
  std::atomic<int> phase_;
  std::atomic<ValueType> progress_;
  std::atomic<ValueType> total_;
};

}  // namespace ky::observability

#endif  // KSYNC_SRC_KY_OBSERVABILITY_INCLUDE_KY_OBSERVABILITY_OBSERVABLE_H
