#ifndef KSYNC_SRC_TESTS_HTTP_SERVER_NGINX_SERVER_H
#define KSYNC_SRC_TESTS_HTTP_SERVER_NGINX_SERVER_H

#include <filesystem>
#include <thread>

#include "../../metrics/metric_container.h"

namespace kysync {

class NginxServer final : public MetricContainer {
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

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_SRC_TESTS_HTTP_SERVER_NGINX_SERVER_H
