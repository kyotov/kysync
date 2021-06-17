#ifndef KSYNC_SRC_TESTS_HTTP_SERVER_NGINX_SERVER_H
#define KSYNC_SRC_TESTS_HTTP_SERVER_NGINX_SERVER_H

#include <ky/metrics/metrics.h>

#include <filesystem>
#include <thread>

namespace kysync {

class NginxServer final : public ky::metrics::MetricContainer {
public:
  NginxServer(std::filesystem::path root, int port);

  virtual ~NginxServer();

private:
  std::filesystem::path root_;
  int port_;
  bool running_;
  std::thread server_;

  void Start();
  void Stop();

  void Accept(ky::metrics::MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_HTTP_SERVER_NGINX_SERVER_H
