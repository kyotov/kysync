#include <ky/metrics/metric_callback_visitor.h>

#include <sstream>

namespace ky::metrics {

void MetricCallbackVisitor::Visit(
    const std::string &name,
    MetricContainer &container) {
  context_.push_back(name);
  container.Accept(*this);
  context_.pop_back();
}

void MetricCallbackVisitor::Visit(const std::string &name, Metric &value) {
  std::stringstream s;
  s << "//";
  for (const auto &term : context_) {
    s << term << "/";
  }
  s << name;
  callback_(s.str(), value);
}

void MetricCallbackVisitor::Snapshot(
    Callback callback,
    MetricContainer &container) {
  callback_ = std::move(callback);
  context_.clear();
  context_.emplace_back(typeid(container).name());
  container.Accept(*this);
}

}  // namespace ky::metrics
