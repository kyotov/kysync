#include "ExpectationCheckMetricsVisitor.h"

struct ExpectationCheckMetricVisitor::Impl {
  std::map<std::string, uint64_t> expectations;
  std::map<std::string, uint64_t> unchecked;
  std::string context;

  explicit Impl(std::map<std::string, uint64_t> &&_expectations)
      : expectations(_expectations),
        context("//") {}

  ~Impl() {
    std::stringstream s;

    s << "unmet expectations:" << std::endl;
    for (const auto &kv : expectations) {
      s << "  {\"" << kv.first << "\", " << kv.second << "}," << std::endl;
    }

    s << "unchecked metrics:" << std::endl;
    for (const auto &kv : unchecked) {
      s << "  {\"" << kv.first << "\", " << kv.second << "}," << std::endl;
    }

    EXPECT_EQ(expectations.size(), 0) << s.str();
  }

  void visit(const std::string &name, const Metric &value) {
    auto key = context + name;

    auto i = expectations.find(key);
    if (i != expectations.end()) {
      const auto &expected = i->second;
      const auto &found = value;
      EXPECT_EQ(found, expected) << "for metric " << key;
      expectations.erase(i);
    } else {
      unchecked[key] = value;
    }
  }

  void visit(
      const std::string &name,
      const MetricContainer &container,
      ExpectationCheckMetricVisitor &host) {
    auto old_context = context;
    context += name + "/";
    container.Accept(host);
    context = old_context;
  }
};

ExpectationCheckMetricVisitor::ExpectationCheckMetricVisitor(
    MetricContainer &host,
    std::map<std::string, uint64_t> &&_expectations)
    : pImpl(std::make_unique<Impl>(std::move(_expectations))) {
  host.Accept(*this);
}

ExpectationCheckMetricVisitor::~ExpectationCheckMetricVisitor() = default;

void ExpectationCheckMetricVisitor::visit(
    const std::string &name,
    const Metric &value) {
  pImpl->visit(name, value);
}

void ExpectationCheckMetricVisitor::visit(
    const std::string &name,
    const MetricContainer &container) {
  pImpl->visit(name, container, *this);
}
