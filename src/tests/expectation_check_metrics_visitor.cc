#include "expectation_check_metrics_visitor.h"

namespace kysync {

ExpectationCheckMetricVisitor::ExpectationCheckMetricVisitor(
    MetricContainer &host,
    std::map<std::string, uint64_t> &&expectations)
    : expectations_(expectations),
      context_("//") {
  host.Accept(*this);
}

ExpectationCheckMetricVisitor::~ExpectationCheckMetricVisitor() {
  std::stringstream s;

  s << "unmet expectations:" << std::endl;
  for (const auto &kv : expectations_) {
    s << "  {\"" << kv.first << "\", " << kv.second << "}," << std::endl;
  }

  s << "unchecked metrics:" << std::endl;
  for (const auto &kv : unchecked_) {
    s << "  {\"" << kv.first << "\", " << kv.second << "}," << std::endl;
  }

  EXPECT_EQ(expectations_.size(), 0) << s.str();
};

void ExpectationCheckMetricVisitor::Visit(
    const std::string &name,
    Metric &value) {
  auto key = context_ + name;

  auto i = expectations_.find(key);
  if (i != expectations_.end()) {
    const auto &expected = i->second;
    const auto &found = value;
    EXPECT_EQ(found, expected) << "for metric " << key;
    expectations_.erase(i);
  } else {
    unchecked_[key] = value;
  }
}

void ExpectationCheckMetricVisitor::Visit(
    const std::string &name,
    MetricContainer &container) {
  auto old_context = context_;
  context_ += name + "/";
  container.Accept(*this);
  context_ = old_context;
}

}  // namespace kysync