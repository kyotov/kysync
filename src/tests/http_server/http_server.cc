#include "http_server.h"

#include <glog/logging.h>
#include <httplib.h>

#include <filesystem>
#include <future>
#include <sstream>
#include <utility>

#include "../../metrics/metric.h"
#include "../../metrics/metric_visitor.h"

namespace kysync {

namespace fs = std::filesystem;

using namespace httplib;

class HttpServer::Impl : public MetricContainer {
public:
  const fs::path kRoot;
  const int kPort;
  const bool kLogHeaders;
  std::unique_ptr<Server> server;
  std::thread thread;

  Metric requests{0};
  Metric total_bytes{0};

  Impl(fs::path root, int port, bool log_headers)
      : kRoot(std::move(root)),
        kPort(port),
        kLogHeaders(log_headers),
        server(std::make_unique<Server>()) {
    Init();
  }

  Impl(const fs::path& cert_path, fs::path root, int port, bool log_headers)
      : kRoot(std::move(root)),
        kPort(port),
        kLogHeaders(log_headers),
        server(std::make_unique<SSLServer>(
            (cert_path / "cert.pem").string().c_str(),
            (cert_path / "key.pem").string().c_str())) {
    Init();
  }

  ~Impl() {
    server->stop();
    thread.join();
  }

  void Logger(const Request& req, const Response& res) {
    requests++;
    total_bytes += res.template get_header_value<uint64_t>("Content-Length");

    if (kLogHeaders) {
      std::stringstream s;
      s << std::endl << "--- Request Headers ---" << std::endl;
      for (const auto& x : req.headers) {
        s << x.first << " " << x.second << std::endl;
      }
      s << std::endl << "--- Response Headers ---" << std::endl;
      for (const auto& x : res.headers) {
        s << x.first << " " << x.second << std::endl;
      }
      s << std::endl;
      LOG(INFO) << s.str();
    }
  }

  void Init() {
    server->set_mount_point("/", kRoot.string().c_str());

    server->set_logger([this](auto req, auto res) { Logger(req, res); });

    thread = std::thread([this]() {
      CHECK(server->listen("localhost", kPort)) << "server started!";
    });
  }

  void Accept(MetricVisitor& visitor) override {
    VISIT_METRICS(requests);
    VISIT_METRICS(total_bytes);
  };
};

HttpServer::HttpServer(fs::path root, int port, bool log_headers)
    : impl_(std::make_unique<Impl>(std::move(root), port, log_headers)) {}

HttpServer::HttpServer(
    const fs::path& cert_path,
    fs::path root,
    int port,
    bool log_headers)
    : impl_(std::make_unique<Impl>(
          cert_path,
          std::move(root),
          port,
          log_headers)) {}

HttpServer::~HttpServer() noexcept = default;

void HttpServer::Accept(MetricVisitor& visitor) { impl_->Accept(visitor); }

}  // namespace kysync
