#ifndef KSYNC_MONITOR_H
#define KSYNC_MONITOR_H

#include <functional>
#include <list>
#include <stack>
#include <unordered_map>
#include <vector>

#include "commands/command.h"
#include "metrics/metric_visitor.h"
#include "utilities/timer.h"

namespace kysync {

// FIXME: I'd really have liked to extend private MetricVisitor but clang barfs!
class Monitor final : public MetricVisitor {
  struct Phase {
    Metric bytes{};
    Metric ms{};
  };

  std::list<Phase> phases_;

  std::vector<std::string> metric_keys_;
  std::unordered_map<std::string, Metric *> metrics_;

  std::stack<std::string> context_;

  Command &command_;

  Timestamp ts_total_begin_;
  Timestamp ts_phase_begin_;

  void Update();

public:
  explicit Monitor(Command &command);

  int Run();

  void RegisterMetricContainer(
      const std::string &name,
      MetricContainer &metric_container);

  using MetricCallback =
      std::function<void(const std::string &, MetricValueType)>;
  void MetricSnapshot(const MetricCallback &callback);

  void Visit(const std::string &name, Metric &value) override;
  void Visit(const std::string &name, MetricContainer &container) override;
};

}  // namespace kysync

#endif  // KSYNC_MONITOR_H
