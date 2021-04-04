#include "Monitor.h"

#include <glog/logging.h>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "metrics/MetricVisitor.h"

class Monitor::Impl : public MetricVisitor {
public:
  std::vector<std::string> metric_keys_;
  std::unordered_map<std::string, const Metric*> metrics_;

  std::stack<std::string> context_;

  Command& command_;

  uint64_t phase_ = 0;
  std::string last_update_;

  decltype(std::chrono::high_resolution_clock::now()) ts_total_begin_;
  decltype(std::chrono::high_resolution_clock::now()) ts_phase_begin_;

  explicit Impl(Command& command) : command_(command) { context_.push(""); }

  void Visit(const std::string& name, const Metric& value) override {
    auto key = context_.top() + "/" + name;
    metric_keys_.push_back(key);
    metrics_[key] = &value;
  }

  void Visit(const std::string& name, const MetricContainer& container)
      override {
    context_.push(context_.top() + "/" + name);
    container.Accept(*this);
    context_.pop();
  }

  void Dump() {
    for (const auto& key : metric_keys_) {
      LOG(INFO) << key << "=" << metrics_[key]->load();
    }
  }

  void Update(bool last) {
    auto size = metrics_["//progress_total_bytes_"]->load();
    auto position = metrics_["//progress_current_bytes_"]->load();

    using ms = std::chrono::milliseconds;
    auto now = std::chrono::high_resolution_clock::now();

    auto total_duration = now - ts_total_begin_;
    auto phase_duration = now - ts_phase_begin_;
    auto total_s = duration_cast<ms>(total_duration).count() / 1000.0;
    auto phase_s = duration_cast<ms>(phase_duration).count() / 1000.0;
    auto percent = size == 0 ? 0 : 100 * position / size;
    auto mbps = phase_s == 0 ? 0 : position / phase_s / (1ll << 20);

    auto new_phase = metrics_["//progress_phase_"]->load();
    if (phase_ != new_phase) {
      if (phase_ > 0) {
        LOG(INFO) << last_update_;
      }
      phase_ = new_phase;
      ts_phase_begin_ = now;
    }

    std::stringstream ss;
    ss << "phase " << phase_ << std::fixed                                 //
       << " | " << std::setw(5) << position / (1ll << 20) << " MB"         //
       << " | " << std::setw(5) << std::setprecision(1) << phase_s << "s"  //
       << " | " << std::setw(7) << mbps << " MB/s"                         //
       << " | " << std::setw(3) << percent << "%"                          //
       << " | " << std::setw(5) << total_s << "s total";
    last_update_ = ss.str();
    std::cout << last_update_ << "\t\r";

    if (last) {
      LOG(INFO) << last_update_;
      Dump();
    }
  }

  int Run() {
    Visit("", command_);

    ts_total_begin_ = std::chrono::high_resolution_clock::now();

    auto result = std::async([this]() { return command_.Run(); });

    std::chrono::milliseconds period(100);

    while (result.wait_for(period) != std::future_status::ready) {
      Update(false);
    }
    Update(true);

    return result.get();
  }
};

Monitor::Monitor(Command& command) : impl_(std::make_unique<Impl>(command)) {}

Monitor::~Monitor() = default;

int Monitor::Run() { return impl_->Run(); }