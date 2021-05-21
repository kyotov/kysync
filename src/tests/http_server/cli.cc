#include <glog/logging.h>

#include <filesystem>
#include <iostream>

#include "http_server.h"

int main() {
  auto http_server = kysync::HttpServer(  //
      ".",
      std::filesystem::current_path(),
      8000,
      true);

  LOG(INFO) << "started http_server server... press ENTER to stop it.";
  std::cin.ignore();
  LOG(INFO) << "stopping http_server server...";
}