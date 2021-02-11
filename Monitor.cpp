#include "Monitor.h"

#include <glog/logging.h>

#include <chrono>
#include <future>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "metrics/MetricVisitor.h"

struct Monitor::Impl : public MetricVisitor {
  std::vector<std::string> metricKeys;
  std::unordered_map<std::string, const Metric*> metrics;

  std::stack<std::string> context;

  Command& command;

  decltype(std::chrono::high_resolution_clock::now()) ts_begin;

  explicit Impl(Command& _command) : command(_command)
  {
    context.push("");
  }

  void visit(const std::string& name, const Metric& value) override
  {
    auto key = context.top() + "/" + name;
    metricKeys.push_back(key);
    metrics[key] = &value;
  }

  void visit(const std::string& name, const MetricContainer& container) override
  {
    context.push(context.top() + "/" + name);
    container.accept(*this);
    context.pop();
  }

  void dump()
  {
    for (const auto& key : metricKeys) {
      LOG(INFO) << key << "=" << metrics[key]->load();
    }
  }

  void update(bool last)
  {
    auto position = metrics["//progressCurrentBytes"]->load();
    auto size = metrics["//progressTotalBytes"]->load();

    auto duration = std::chrono::high_resolution_clock::now() - ts_begin;
    auto s = duration_cast<std::chrono::milliseconds>(duration).count() / 100;
    auto percent = size == 0 ? 0 : 100 * position / size;
    auto mbps = s == 0 ? 0 : 10.0 * position / s / (1ll << 20);

    std::cout << "\r" << (last ? "done!     " : "working...");
    std::cout << " | " << position / (1ll << 20) << "MB";
    std::cout << " | " << s / 10.0 << "s";
    std::cout << " | " << percent << "%";
    std::cout << " | " << mbps << " MB/s\r";

    if (last) {
      std::cout << std::endl;
      dump();
    }
  }

  int run()
  {
    visit("", command);

    ts_begin = std::chrono::high_resolution_clock::now();
    auto result = std::async([this]() { return command.run(); });

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

int Monitor::run()
{
  return pImpl->run();
}