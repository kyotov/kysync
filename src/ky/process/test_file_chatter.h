#ifndef KSYNC_SRC_KY_PROCESS_TEST_FILE_CHATTER_H
#define KSYNC_SRC_KY_PROCESS_TEST_FILE_CHATTER_H

#include <ky/timer.h>

#include <filesystem>
#include <mutex>

namespace ky {

class TestFileChatter final {
public:
  TestFileChatter(
      std::string name,
      std::filesystem::path input_path,
      std::filesystem::path output_path);

  void Write(const std::string &message);
  std::string Read(ky::timer::Milliseconds timeout);
  std::string Read();

private:
  std::string name_;
  std::streamoff input_offset_;
  std::filesystem::path input_path_;
  std::filesystem::path output_path_;
};

}  // namespace ky

#endif  // KSYNC_SRC_KY_PROCESS_TEST_FILE_CHATTER_H
