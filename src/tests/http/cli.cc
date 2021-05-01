#include <glog/logging.h>

#include <filesystem>
#include <iostream>

#include "http_server.h"

int main() {
  auto h = kysync::HttpServer(std::filesystem::current_path());

  LOG(INFO) << "started http server... press ENTER to stop it.";
  std::cin.ignore();
  LOG(INFO) << "stopping http server...";
}
