#ifndef KSYNC_SERVER_H
#define KSYNC_SERVER_H

#include <filesystem>

#include "../../utilities/utilities.h"

namespace kysync {

class HttpServer final {
  PIMPL;
  NO_COPY_OR_MOVE(HttpServer);

public:
  explicit HttpServer(const std::filesystem::path &root);

  ~HttpServer() noexcept;
};

}  // namespace kysync

#endif  // KSYNC_SERVER_H
