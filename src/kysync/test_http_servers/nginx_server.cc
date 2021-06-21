#include <glog/logging.h>
#include <ky/noexcept.h>
#include <kysync/path_config.h>
#include <kysync/test_http_servers/nginx_server.h>

namespace kysync {

NginxServer::NginxServer(std::filesystem::path root, int port)
    : root_(std::move(root)),
      port_(port),
      running_(false) {
  Start();
}

NginxServer::~NginxServer() {
  ky::NoExcept([this]() { Stop(); });
};

static void Execute(const std::string &command) {
  LOG(INFO) << command;

  // NOLINTNEXTLINE(cert-env33-c,concurrency-mt-unsafe)
  CHECK_EQ(0, system(command.c_str())) << command;
}

void NginxServer::Start() {
  std::filesystem::create_directories(root_ / "conf");
  std::filesystem::create_directories(root_ / "logs");
  std::filesystem::create_directories(root_ / "temp");

  auto command = std::stringstream();
  // NOTE: the "echo -- && ..." is required because `system` has trouble running
  //       executables that have spaces in the path name, even if properly
  //       quoted on windows... e.g. `system("\"c:\\program files\\...\"")
  command << "echo -- && \"" << CMAKE_COMMAND.string() << "\"";
  command << " -D PORT=" << port_;
  command << " -D CONF_TEMPLATE_PATH="
          << NGINX_CONF_TEMPLATE_DIR / "nginx.conf.in";
  command << " -D TARGET_DIR=" << root_ / "conf";
  command << " -P " << NGINX_CONF_TEMPLATE_DIR / "nginx_server_conf.cmake";

  Execute(command.str());

  std::filesystem::current_path(root_);
  server_ = std::thread([]() { Execute(NGINX_COMMAND.string()); });

  LOG(INFO) << "waiting for nginx to start...";

  while (!std::filesystem::exists(root_ / "logs" / "nginx.pid")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  running_ = true;
}

void NginxServer::Stop() {
  if (running_) {
    running_ = false;
    Execute(NGINX_COMMAND.string() + " -s stop");
    server_.join();
  }
}

void NginxServer::Accept(ky::metrics::MetricVisitor &visitor) {}

}  // namespace kysync
