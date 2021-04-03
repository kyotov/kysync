#include "ExpectationCheckMetricsVisitor.h"

class ExpectationCheckMetricVisitor::Impl {
public:
  std::map<std::string, uint64_t> expectations_;
  std::map<std::string, uint64_t> unchecked_;
  std::string context_;

  explicit Impl(std::map<std::string, uint64_t> &&_expectations)
      : expectations_(_expectations),
        context_("//") {}

  ~Impl() {
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
  }

  void Visit(const std::string &name, const Metric &value) {
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

  void Visit(
      const std::string &name,
      const MetricContainer &container,
      ExpectationCheckMetricVisitor &host) {
    auto old_context = context_;
    context_ += name + "/";
    container.Accept(host);
    context_ = old_context;
  }
};

ExpectationCheckMetricVisitor::ExpectationCheckMetricVisitor(
    MetricContainer &host,
    std::map<std::string, uint64_t> &&expectations)
    : impl_(std::make_unique<Impl>(std::move(expectations))) {
  host.Accept(*this);
}

ExpectationCheckMetricVisitor::~ExpectationCheckMetricVisitor() = default;

void ExpectationCheckMetricVisitor::Visit(
    const std::string &name,
    const Metric &value) {
  impl_->Visit(name, value);
}

void ExpectationCheckMetricVisitor::Visit(
    const std::string &name,
    const MetricContainer &container) {
  impl_->Visit(name, container, *this);
}
