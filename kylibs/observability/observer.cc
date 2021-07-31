#include <glog/logging.h>
#include <ky/observability/observer.h>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <unordered_map>

namespace ky::observability {

Observer::Observer(Observable& observable) : observable_(observable) {}

void Observer::Update() {
  auto now = std::chrono::high_resolution_clock::now();

  auto total_bytes = observable_.GetTotal();
  auto processed_bytes = observable_.GetProgress();

  auto total_ms = ky::timer::DeltaMs(ts_total_begin_, now);
  auto phase_ms = ky::timer::DeltaMs(ts_phase_begin_, now);
  auto percent = total_bytes != 0 ? 100 * processed_bytes / total_bytes : 0;
  auto phase_ms_f = static_cast<double>(phase_ms);
  auto total_ms_f = static_cast<double>(total_ms);
  auto mb = 1.0 * static_cast<double>(processed_bytes) / (1 << 20);
  auto mbps = phase_ms != 0 ? 1000.0 * mb / phase_ms_f : 0;

  std::stringstream ss;
  ss << "phase " << observable_.GetPhase()                //
     << std::fixed << std::setprecision(1)                //
     << " | " << std::setw(5) << mb << " MB"              //
     << " | " << std::setw(5) << phase_ms_f / 1e3 << "s"  //
     << " | " << std::setw(7) << mbps << " MB/s"          //
     << " | " << std::setw(3) << percent << "%"           //
     << " | " << std::setw(5) << total_ms_f / 1e3 << "s total";
  std::cout << ss.str() << "\t\r";
  std::cout.flush();

  if (observable_.IsReadyForNextPhase()) {
    LOG(INFO) << ss.str();

    phases_.emplace_back();
    phases_.back().bytes = processed_bytes;
    phases_.back().ms = ky::timer::DeltaMs(ts_phase_begin_, now);

    observable_.AdvancePhase();
    ts_phase_begin_ = now;
  }
}

int Observer::Run(const std::function<int(void)>& task) {
  observable_.EnableMonitor();

  ts_total_begin_ = std::chrono::high_resolution_clock::now();
  ts_phase_begin_ = ts_total_begin_;

  auto result = std::async(task);

  ky::timer::Milliseconds period(100);

  while (result.wait_for(period) != std::future_status::ready) {
    Update();
  }

  SnapshotPhases(
      [](auto key, auto value) { LOG(INFO) << key << "=" << value; });

  return result.get();
}

void Observer::SnapshotPhases(
    const std::function<
        void(const std::string& key, const Observable::ValueType& value)>&
        callback) {
  int phase_index = 0;
  for (auto& phase : phases_) {
    auto phase_key_prefix = std::string("//") + observable_.GetName() +
                            "/phase_" + std::to_string(phase_index);
    callback(phase_key_prefix + "_bytes", phase.bytes);
    callback(phase_key_prefix + "_ms", phase.ms);
    phase_index++;
  }
}

}  // namespace ky::observability