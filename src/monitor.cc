#include "monitor.h"

#include <glog/logging.h>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <stack>
#include <unordered_map>

#include "metrics/metric_visitor.h"
#include "utilities/timer.h"

namespace kysync {

class Monitor::Impl : public MetricVisitor {
public:
  struct Phase {
    Metric bytes;
    Metric ms;
  };

  std::list<Phase> phases_;

  std::vector<std::string> metric_keys_;
  std::unordered_map<std::string, Metric*> metrics_;

  std::stack<std::string> context_;

  Command& command_;

  Timestamp ts_total_begin_;
  Timestamp ts_phase_begin_;

  explicit Impl(Command& command) : command_(command) { context_.push(""); }

  void Visit(const std::string& name, Metric& value) override {
    auto key = context_.top() + "/" + name;
    metric_keys_.push_back(key);
    metrics_[key] = &value;
  }

  void Visit(const std::string& name, MetricContainer& container) override {
    context_.push(context_.top() + "/" + name);
    container.Accept(*this);
    context_.pop();
  }

  void Update() {
    auto& phase = *metrics_["//progress_phase_"];
    auto& total_bytes = *metrics_["//progress_total_bytes_"];
    auto& processed_bytes = *metrics_["//progress_current_bytes_"];

    auto now = std::chrono::high_resolution_clock::now();

    auto total_ms = DeltaMs(ts_total_begin_, now);
    auto phase_ms = DeltaMs(ts_phase_begin_, now);
    auto percent = total_bytes != 0 ? 100 * processed_bytes / total_bytes : 0;
    auto mb = processed_bytes / (1LL << 20);
    auto mbps = phase_ms != 0 ? 1000.0 * mb / phase_ms : 0;

    std::stringstream ss;
    ss << "phase " << phase << std::fixed << std::setprecision(1)  //
       << " | " << std::setw(5) << mb << " MB"                     //
       << " | " << std::setw(5) << phase_ms / 1e3 << "s"           //
       << " | " << std::setw(7) << mbps << " MB/s"                 //
       << " | " << std::setw(3) << percent << "%"                  //
       << " | " << std::setw(5) << total_ms / 1e3 << "s total";
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

  int Run() {
    Visit("", command_);
    metrics_["//progress_monitor_enabled_"]->store(1);

    ts_total_begin_ = std::chrono::high_resolution_clock::now();

    auto result = std::async([this]() { return command_.Run(); });

    Milliseconds period(100);

    while (result.wait_for(period) != std::future_status::ready) {
      Update();
    }

    return result.get();
  }
};

Monitor::Monitor(Command& command) : impl_(std::make_unique<Impl>(command)) {}

Monitor::~Monitor() = default;

int Monitor::Run() {
  auto result = impl_->Run();
  MetricSnapshot(
      [](auto key, auto value) { LOG(INFO) << key << "=" << value; });
  return result;
}

void Monitor::RegisterMetricContainer(
    const std::string& name,
    MetricContainer& metric_container) {
  impl_->Visit(name, metric_container);
}

void Monitor::MetricSnapshot(const Monitor::MetricCallback& callback) const {
  for (const auto& key : impl_->metric_keys_) {
    callback(key, impl_->metrics_[key]->load());
  }
}

}  // namespace kysync