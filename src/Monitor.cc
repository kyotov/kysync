#include "Monitor.h"

#include <glog/logging.h>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "metrics/MetricVisitor.h"

namespace kysync {

class Monitor::Impl : public MetricVisitor {
public:
  using TimeStamp = decltype(std::chrono::high_resolution_clock::now());
  using Milliseconds = std::chrono::milliseconds;

  struct Phase {
    Metric::value_type total_bytes;
    Metric::value_type processed_bytes;
    Metric::value_type elapsed_ms;
  };

  std::vector<Phase> phases;

  std::vector<std::string> metric_keys_;
  std::unordered_map<std::string, Metric*> metrics_;

  std::stack<std::string> context_;

  Command& command_;

  std::string last_update_;

  TimeStamp ts_total_begin_;
  TimeStamp ts_phase_begin_;

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

  void Dump() {
    for (const auto& key : metric_keys_) {
      LOG(INFO) << key << "=" << metrics_[key]->load();
    }
    auto phase = 0;
    for (const auto& v : phases) {
      phase++;
      LOG(INFO) << "//phases/" << phase << "/total_bytes=" << v.total_bytes;
      LOG(INFO) << "//phases/" << phase
                << "/processed_bytes=" << v.processed_bytes;
      LOG(INFO) << "//phases/" << phase << "/elapsed_ms=" << v.elapsed_ms;
    }
  }

  void Update() {
    static auto& phase = *metrics_["//progress_phase_"];
    static auto& total_bytes = *metrics_["//progress_total_bytes_"];
    static auto& processed_bytes = *metrics_["//progress_current_bytes_"];

    auto now = std::chrono::high_resolution_clock::now();

    auto delta_ms = [&](auto duration) {
      return static_cast<size_t>(duration_cast<Milliseconds>(duration).count());
    };

    auto total_ms = delta_ms(now - ts_total_begin_);
    auto phase_ms = delta_ms(now - ts_phase_begin_);
    auto percent = total_bytes != 0 ? 100 * processed_bytes / total_bytes : 0;
    auto mb = processed_bytes / (1LL << 20);
    auto mbps = phase_ms != 0 ? 1000.0 * mb / phase_ms : 0;

    std::stringstream ss;
    ss << "phase " << phase << std::fixed << std::setprecision(1)  //
       << " | " << std::setw(5) << mb << " MB"                     //
       << " | " << std::setw(5) << phase_ms / 1000.0 << "s"        //
       << " | " << std::setw(7) << mbps << " MB/s"                 //
       << " | " << std::setw(3) << percent << "%"                  //
       << " | " << std::setw(5) << total_ms / 1000.0 << "s total";
    std::cout << ss.str() << "\t\r";

    if (metrics_["//progress_next_phase_"]->load() !=
        metrics_["//progress_phase_"]->load())
    {
      if (phase > 0) {
        LOG(INFO) << ss.str();
        phases.push_back(
            {.total_bytes = total_bytes,
             .processed_bytes = processed_bytes,
             .elapsed_ms = delta_ms(now - ts_phase_begin_)});
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

    std::chrono::milliseconds period(100);

    while (result.wait_for(period) != std::future_status::ready) {
      Update();
    }
    Dump();

    return result.get();
  }
};

Monitor::Monitor(Command& command) : impl_(std::make_unique<Impl>(command)) {}

Monitor::~Monitor() = default;

int Monitor::Run() { return impl_->Run(); }

}  // namespace kysync