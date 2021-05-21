#ifndef KSYNC_HTTP_SERVER_H
#define KSYNC_HTTP_SERVER_H

#include <filesystem>

#include "../../metrics/metric_container.h"
#include "../../utilities/utilities.h"

namespace kysync {

class HttpServer final : public MetricContainer {
  PIMPL;
  NO_COPY_OR_MOVE(HttpServer);

public:
  HttpServer(std::filesystem::path root, int port, bool log_headers);

  HttpServer(
      const std::filesystem::path& cert_path,
      std::filesystem::path root,
      int port,
      bool log_headers);

  ~HttpServer() noexcept;

  void Accept(MetricVisitor& visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_HTTP_SERVER_H
