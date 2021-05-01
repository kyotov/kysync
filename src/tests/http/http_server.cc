#include "http_server.h"

#include <glog/logging.h>
#include <httplib.h>

#include <filesystem>
#include <future>
#include <utility>
#include <sstream>

#include "../../metrics/metric.h"
#include "../../metrics/metric_visitor.h"

namespace kysync {

namespace fs = std::filesystem;

using namespace httplib;

struct HttpServer::Impl : public MetricContainer {
  const fs::path kRoot;
  const int kPort;
  std::unique_ptr<Server> server;
  std::thread thread;

  Metric requests{0};
  Metric total_bytes{0};

  Impl(fs::path root, int port)
      : kRoot(std::move(root)),
        kPort(port),
        server(std::make_unique<Server>()) {
    Init();
  }

  Impl(const fs::path& cert_path, fs::path root, int port)
      : kRoot(std::move(root)),
        kPort(port),
        server(std::make_unique<SSLServer>(
            (cert_path / "cert.pem").string().c_str(),
            (cert_path / "key.pem").string().c_str())) {
    Init();
  }

  ~Impl() {
    server->stop();
    thread.join();
  }

  void Init() {
    server->set_mount_point("/", kRoot.string().c_str());

    server->set_logger([&](const auto& req, auto& res) {
      requests++;
      total_bytes += res.template get_header_value<uint64_t>("Content-Length");
      // FIXME: debugging of http server statistics
      //      if (req.method == "GET") {
      //        std::stringstream s;
      //        s << total_bytes << std::endl;
      //        s << req.method << std::endl;
      //        s << res.status << std::endl;
      //        s << "---" << std::endl;
      //        for (const auto& x : req.headers) {
      //          s << x.first << " " << x.second << std::endl;
      //        }
      //        s << "---" << std::endl;
      //        for (const auto& x : res.headers) {
      //          s << x.first << " " << x.second << std::endl;
      //        }
      //        LOG(INFO) << s.str();
      //      }
    });

    thread = std::thread([&]() {
      CHECK(server->listen("localhost", kPort)) << "server started!";
    });
  }

  void Accept(MetricVisitor& visitor) override {
    VISIT_METRICS(requests);
    VISIT_METRICS(total_bytes);
  };
};

HttpServer::HttpServer(const fs::path& root, int port)
    : impl_(std::make_unique<Impl>(root, port)) {}

HttpServer::HttpServer(
    const std::filesystem::path& cert_path,
    const std::filesystem::path& root,
    int port)
    : impl_(std::make_unique<Impl>(cert_path, root, port)) {}

HttpServer::~HttpServer() noexcept = default;

void HttpServer::Accept(MetricVisitor& visitor) { impl_->Accept(visitor); }

}  // namespace kysync
