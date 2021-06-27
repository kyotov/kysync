#ifndef KSYNC_SRC_TESTS_HTTP_SERVER_HTTP_SERVER_H
#define KSYNC_SRC_TESTS_HTTP_SERVER_HTTP_SERVER_H

#include <ky/metrics/metrics.h>

#include <filesystem>
#include <thread>

namespace httplib {
class Server;
class Request;
class Response;
}  // namespace httplib

namespace kysync {

class HttpServer final : public ky::metrics::MetricContainer {
  std::filesystem::path root_;
  int port_{};
  bool log_headers_;
  std::unique_ptr<httplib::Server> server_;
  std::thread thread_;

  ky::metrics::Metric requests_{};
  ky::metrics::Metric bytes_{};

  void Logger(const httplib::Request& req, const httplib::Response& res);
  void Start();

public:
  HttpServer(std::filesystem::path root, bool log_headers);

  HttpServer(
      const std::filesystem::path& cert_path,
      std::filesystem::path root,
      bool log_headers);

  int GetPort() const;

  void Stop();

  ~HttpServer();

  void Accept(ky::metrics::MetricVisitor& visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_HTTP_SERVER_HTTP_SERVER_H
