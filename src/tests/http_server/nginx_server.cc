#include "nginx_server.h"

#include <glog/logging.h>
#include <kysync/path_config.h>

namespace kysync {

NginxServer::NginxServer(std::filesystem::path root, int port)
    : root_(std::move(root)),
      port_(port) {
  Start();
}

NginxServer::~NginxServer() {
  Stop();
}

static void Execute(const std::string &command) {
  LOG(INFO) << command;

  CHECK_EQ(0, system(command.c_str()));
}

void NginxServer::Start() {
  std::filesystem::create_directories(root_ / "conf");
  std::filesystem::create_directories(root_ / "logs");
  std::filesystem::create_directories(root_ / "temp");

  auto src_dir = CMAKE_SOURCE_DIR / "src" / "tests" / "http_server";

  auto command = std::stringstream();
  // NOTE: the "echo -- && ..." is required because `system` has trouble running
  //       executables that have spaces in the path name, even if properly
  //       quoted on windows... e.g. `system("\"c:\\program files\\...\"")
  command << "echo -- && \"" << CMAKE_COMMAND.string() << "\"";
  command << " -D PORT=" << port_;
  command << " -D CONF_TEMPLATE_PATH=" << src_dir / "nginx.conf.in";
  command << " -D TARGET_DIR=" << root_ / "conf";
  command << " -P " << src_dir / "nginx_server_conf.cmake";

  Execute(command.str());

  std::filesystem::current_path(root_);
  server_ = std::thread([]() { Execute(NGINX_COMMAND.string()); });

  LOG(INFO) << "waiting for nginx to start...";

  while (!std::filesystem::exists(root_ / "logs" / "nginx.pid")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void NginxServer::Stop() {
  Execute(NGINX_COMMAND.string() + " -s stop");
  server_.join();
}

void NginxServer::Accept(MetricVisitor &visitor) {}

}  // namespace kysync
