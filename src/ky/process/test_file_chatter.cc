#include "test_file_chatter.h"

#include <glog/logging.h>

#include <chrono>
#include <fstream>
#include <thread>
#include <utility>

namespace ky {

using namespace std::chrono_literals;

TestFileChatter::TestFileChatter(
    std::string name,
    std::filesystem::path input_path,
    std::filesystem::path output_path)
    : name_(std::move(name)),
      input_offset_(0),
      input_path_(std::move(input_path)),
      output_path_(std::move(output_path)) {}

void TestFileChatter::Write(const std::string &message) {
  auto s = std::ofstream(output_path_, std::ios::app);
  s << message << std::endl;
  LOG(INFO) << name_ << "|wrote|" << message;
}

std::string TestFileChatter::Read(ky::timer::Milliseconds timeout) {
  std::string line;

  for (auto beg = ky::timer::Now();
       ky::timer::DeltaMs(beg, ky::timer::Now()) < timeout.count();)
  {
    if (std::filesystem::exists(input_path_)) {
      auto s = std::ifstream(input_path_);
      CHECK(s);
      s.seekg(input_offset_);
      CHECK(s);
      std::getline(s, line);

      if (!line.empty()) {
        input_offset_ = s.tellg();
        LOG(INFO) << name_ << "|read|" << line;
        break;
      }
    }
    std::this_thread::sleep_for(10ms);
  }

  return std::move(line);
}

std::string TestFileChatter::Read() { return Read(1000ms); }

}  // namespace ky
