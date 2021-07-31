#include <ky/observability/observable.h>

namespace ky::observability {

Observable::Observable(std::string name)
    : name_(std::move(name)),
      is_monitored_(false),
      is_ready_for_next_phase_(false),
      phase_(0),
      progress_(0),
      total_(1) {}

const std::string& Observable::GetName() const { return name_; }

int Observable::GetPhase() const { return phase_; }

void Observable::AdvancePhase() {
  is_ready_for_next_phase_ = false;
  phase_++;
}

bool Observable::IsReadyForNextPhase() const {
  return is_ready_for_next_phase_;
}

Observable::ValueType Observable::GetTotal() const { return total_; }

Observable::ValueType Observable::GetProgress() const { return progress_; }

void Observable::AdvanceProgress(Observable::ValueType delta) {
  progress_ += delta;
}

void Observable::EnableMonitor() { is_monitored_ = true; }

void Observable::StartNextPhase(Observable::ValueType total) {
  is_ready_for_next_phase_ = true;

  if (is_monitored_) {
    auto next_phase = phase_ + 1;
    // wait for monitor to advance to next phase
    while (phase_ != next_phase) {
    }
  } else {
    AdvancePhase();
  }

  progress_ = 0;
  total_ = total;
}

}  // namespace ky::observability
