#include "Monitor.h"

#include <glog/logging.h>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "metrics/MetricVisitor.h"

struct Monitor::Impl : public MetricVisitor {
  std::vector<std::string> metric_keys_;
  std::unordered_map<std::string, const Metric*> metrics_;

  std::stack<std::string> context_;

  Command& command_;

  uint64_t phase_ = 0;
  std::string last_update_;

  decltype(std::chrono::high_resolution_clock::now()) ts_total_begin_;
  decltype(std::chrono::high_resolution_clock::now()) ts_phase_begin_;

  explicit Impl(Command& _command) : command_(_command) { context_.push(""); }

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

  void update(bool last) {
    auto size = metrics_["//progress_total_bytes_"]->load();
    auto position = metrics_["//progress_current_bytes_"]->load();

    using ms = std::chrono::milliseconds;
    auto now = std::chrono::high_resolution_clock::now();

    auto totalDuration = now - ts_total_begin_;
    auto phaseDuration = now - ts_phase_begin_;
    auto totalS = duration_cast<ms>(totalDuration).count() / 1000.0;
    auto phaseS = duration_cast<ms>(phaseDuration).count() / 1000.0;
    auto percent = size == 0 ? 0 : 100 * position / size;
    auto mbps = phaseS == 0 ? 0 : position / phaseS / (1ll << 20);

    auto newPhase = metrics_["//progress_phase_"]->load();
    if (phase_ != newPhase) {
      if (phase_ > 0) {
        LOG(INFO) << last_update_;
      }
      phase_ = newPhase;
      ts_phase_begin_ = now;
    }

    std::stringstream update;
    update << "phase " << phase_ << std::fixed                                 //
           << " | " << std::setw(5) << position / (1ll << 20) << " MB"        //
           << " | " << std::setw(5) << std::setprecision(1) << phaseS << "s"  //
           << " | " << std::setw(7) << mbps << " MB/s"                        //
           << " | " << std::setw(3) << percent << "%"                         //
           << " | " << std::setw(5) << totalS << "s total";
    last_update_ = update.str();
    std::cout << last_update_ << "\t\r";

    if (last) {
      LOG(INFO) << last_update_;
      Dump();
    }
  }

  int run() {
    Visit("", command_);

    ts_total_begin_ = std::chrono::high_resolution_clock::now();

    auto result = std::async([this]() { return command_.Run(); });

    std::chrono::milliseconds period(100);

    while (result.wait_for(period) != std::future_status::ready) {
      update(false);
    }
    update(true);

    return result.get();
  }
};

Monitor::Monitor(Command& command) : pImpl(std::make_unique<Impl>(command)) {}

Monitor::~Monitor() = default;

int Monitor::Run() { return pImpl->run(); }