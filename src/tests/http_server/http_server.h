#ifndef KSYNC_HTTP_SERVER_H
#define KSYNC_HTTP_SERVER_H

#include <filesystem>
#include <thread>

#include "../../metrics/metric_container.h"
#include "../../metrics/metric.h"

namespace httplib {
  class Server;
  class Request;
  class Response;
}

namespace kysync {

class HttpServer final : public MetricContainer {
  const std::filesystem::path kRoot;
  const int kPort;
  const bool kLogHeaders;
  std::unique_ptr<httplib::Server> server;
  std::thread thread;

  Metric requests{};
  Metric total_bytes{};

  void Logger(const httplib::Request& req, const httplib::Response& res);
  void Start();

public:
  HttpServer(std::filesystem::path root, int port, bool log_headers);

  HttpServer(
      const std::filesystem::path& cert_path,
      std::filesystem::path root,
      int port,
      bool log_headers);

  void Stop();

  ~HttpServer();

  void Accept(MetricVisitor& visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_HTTP_SERVER_H
