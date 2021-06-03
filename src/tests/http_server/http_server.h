#ifndef KSYNC_HTTP_SERVER_H
#define KSYNC_HTTP_SERVER_H

#include <filesystem>
#include <thread>

#include "../../metrics/metric.h"
#include "../../metrics/metric_container.h"

namespace httplib {
class Server;
class Request;
class Response;
}  // namespace httplib

namespace kysync {

class HttpServer final : public MetricContainer {
  std::filesystem::path root_;
  int port_;
  bool log_headers_;
  std::unique_ptr<httplib::Server> server_;
  std::thread thread_;

  Metric requests_{};
  Metric bytes_{};

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
