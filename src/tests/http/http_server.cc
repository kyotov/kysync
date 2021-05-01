#include "http_server.h"

#include <httplib.h>

#include <filesystem>
#include <future>
#include <utility>

namespace kysync {

namespace fs = std::filesystem;

using namespace httplib;

struct HttpServer::Impl {
  const fs::path kRoot;
  Server server;
  std::future<void> future;

  explicit Impl(fs::path root) : kRoot(std::move(root)) {
    server.set_mount_point("/", kRoot.string().c_str());
    // TODO: try std::thread instead of std::async
    future = std::async([&]() { server.listen("localhost", 8000); });
  }

  ~Impl() {
    server.stop();
    future.wait();
  }
};

HttpServer::HttpServer(const fs::path& root)
    : impl_(std::make_unique<Impl>(root)) {}

HttpServer::~HttpServer() noexcept = default;

}  // namespace kysync
