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

void HttpServer::Logger(
    const httplib::Request& req,
    const httplib::Response& res) {
  requests_++;
  bytes_ += res.template get_header_value<uint64_t>("Content-Length");

  if (log_headers_) {
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

void HttpServer::Start() {
  server_->set_mount_point("/", root_.string().c_str());

  server_->set_logger([this](auto req, auto res) { Logger(req, res); });

  thread_ = std::thread([this]() {
    CHECK(server_->listen("localhost", port_)) << "server started!";
  });

  // FIXME: this is garbage... we need to figure out how to make sure the server
  // has started!!!
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void HttpServer::Accept(MetricVisitor& visitor) {
  VISIT_METRICS(requests_);
  VISIT_METRICS(bytes_);
};

HttpServer::HttpServer(fs::path root, int port, bool log_headers)
    : root_(std::move(root)),
      port_(port),
      log_headers_(log_headers),
      server_(std::make_unique<httplib::Server>()) {
  Start();
}

HttpServer::HttpServer(
    const fs::path& cert_path,
    fs::path root,
    int port,
    bool log_headers)
    : root_(std::move(root)),
      port_(port),
      log_headers_(log_headers),
      server_(std::make_unique<httplib::SSLServer>(
          (cert_path / "cert.pem").string().c_str(),
          (cert_path / "key.pem").string().c_str())) {
  Start();
}

void HttpServer::Stop() {
  if (server_->is_running()) {
    server_->stop();
    thread_.join();
  }
}

HttpServer::~HttpServer() { Stop(); };

}  // namespace kysync
