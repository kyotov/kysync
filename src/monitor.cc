#include "monitor.h"

#include <glog/logging.h>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <unordered_map>

namespace kysync {

Monitor::Monitor(Command& command) : command_(command) { context_.push(""); }

void Monitor::Visit(const std::string& name, Metric& value) {
  auto key = context_.top() + "/" + name;
  metric_keys_.push_back(key);
  metrics_[key] = &value;
}

void Monitor::Visit(const std::string& name, MetricContainer& container) {
  context_.push(context_.top() + "/" + name);
  container.Accept(*this);
  context_.pop();
}

void Monitor::Update() {
  auto& phase = *metrics_["//progress_phase_"];
  auto& total_bytes = *metrics_["//progress_total_bytes_"];
  auto& processed_bytes = *metrics_["//progress_current_bytes_"];

  auto now = std::chrono::high_resolution_clock::now();

  auto total_ms = DeltaMs(ts_total_begin_, now);
  auto phase_ms = DeltaMs(ts_phase_begin_, now);
  auto percent = total_bytes != 0 ? 100 * processed_bytes / total_bytes : 0;
  auto phase_ms_f = static_cast<double>(phase_ms);
  auto total_ms_f = static_cast<double>(total_ms);
  auto mb = 1.0 * static_cast<double>(processed_bytes) / (1 << 20);
  auto mbps = phase_ms != 0 ? 1000.0 * mb / phase_ms_f : 0;

  std::stringstream ss;
  ss << "phase " << phase << std::fixed << std::setprecision(1)  //
     << " | " << std::setw(5) << mb << " MB"                     //
     << " | " << std::setw(5) << phase_ms_f / 1e3 << "s"           //
     << " | " << std::setw(7) << mbps << " MB/s"                 //
     << " | " << std::setw(3) << percent << "%"                  //
     << " | " << std::setw(5) << total_ms_f / 1e3 << "s total";
  std::cout << ss.str() << "\t\r";
  std::cout.flush();

  if (metrics_["//progress_next_phase_"]->load() !=
      metrics_["//progress_phase_"]->load())
  {
    if (phase > 0) {
      LOG(INFO) << ss.str();

      phases_.emplace_back();
      phases_.back().bytes = processed_bytes.load();
      phases_.back().ms = DeltaMs(ts_phase_begin_, now);

      auto s = std::stringstream();
      s << "//phases/" << phase << "/bytes";
      auto k_bytes = s.str();
      metric_keys_.push_back(k_bytes);
      metrics_[k_bytes] = &phases_.back().bytes;

      s = std::stringstream();
      s << "//phases/" << phase << "/ms";
      auto k_ms = s.str();
      metric_keys_.push_back(k_ms);
      metrics_[k_ms] = &phases_.back().ms;
    }
    phase++;
    ts_phase_begin_ = now;
  }
}

int Monitor::Run() {
  Visit("", command_);
  metrics_["//progress_monitor_enabled_"]->store(1);

  ts_total_begin_ = std::chrono::high_resolution_clock::now();

  auto result = std::async([this]() { return command_.Run(); });

  Milliseconds period(100);

  while (result.wait_for(period) != std::future_status::ready) {
    Update();
  }

  MetricSnapshot(
      [](auto key, auto value) { LOG(INFO) << key << "=" << value; });

  return result.get();
}

void Monitor::RegisterMetricContainer(
    const std::string& name,
    MetricContainer& metric_container) {
  Visit(name, metric_container);
}

void Monitor::MetricSnapshot(const Monitor::MetricCallback& callback) {
  for (const auto& key : metric_keys_) {
    callback(key, metrics_[key]->load());
  }
}

}  // namespace kysync