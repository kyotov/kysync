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
  std::vector<std::string> metricKeys;
  std::unordered_map<std::string, const Metric*> metrics;

  std::stack<std::string> context;

  Command& command;

  uint64_t phase = 0;
  std::string lastUpdate;

  decltype(std::chrono::high_resolution_clock::now()) tsTotalBegin;
  decltype(std::chrono::high_resolution_clock::now()) tsPhaseBegin;

  explicit Impl(Command& _command) : command(_command) { context.push(""); }

  void visit(const std::string& name, const Metric& value) override {
    auto key = context.top() + "/" + name;
    metricKeys.push_back(key);
    metrics[key] = &value;
  }

  void visit(const std::string& name, const MetricContainer& container)
      override {
    context.push(context.top() + "/" + name);
    container.Accept(*this);
    context.pop();
  }

  void dump() {
    for (const auto& key : metricKeys) {
      LOG(INFO) << key << "=" << metrics[key]->load();
    }
  }

  void update(bool last) {
    auto size = metrics["//progressTotalBytes"]->load();
    auto position = metrics["//progressCurrentBytes"]->load();

    using ms = std::chrono::milliseconds;
    auto now = std::chrono::high_resolution_clock::now();

    auto totalDuration = now - tsTotalBegin;
    auto phaseDuration = now - tsPhaseBegin;
    auto totalS = duration_cast<ms>(totalDuration).count() / 1000.0;
    auto phaseS = duration_cast<ms>(phaseDuration).count() / 1000.0;
    auto percent = size == 0 ? 0 : 100 * position / size;
    auto mbps = phaseS == 0 ? 0 : position / phaseS / (1ll << 20);

    auto newPhase = metrics["//progressPhase"]->load();
    if (phase != newPhase) {
      if (phase > 0) {
        LOG(INFO) << lastUpdate;
      }
      phase = newPhase;
      tsPhaseBegin = now;
    }

    std::stringstream update;
    update << "phase " << phase << std::fixed                                 //
           << " | " << std::setw(5) << position / (1ll << 20) << " MB"        //
           << " | " << std::setw(5) << std::setprecision(1) << phaseS << "s"  //
           << " | " << std::setw(7) << mbps << " MB/s"                        //
           << " | " << std::setw(3) << percent << "%"                         //
           << " | " << std::setw(5) << totalS << "s total";
    lastUpdate = update.str();
    std::cout << lastUpdate << "\t\r";

    if (last) {
      LOG(INFO) << lastUpdate;
      dump();
    }
  }

  int run() {
    visit("", command);

    tsTotalBegin = std::chrono::high_resolution_clock::now();

    auto result = std::async([this]() { return command.Run(); });

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

int Monitor::run() { return pImpl->run(); }