#include "file_stream.h"

#include <glog/logging.h>

#include <fstream>

namespace kysync {

// NOTE: The fstream object is consciously initialized with both out and in
// modes although this function only writes to the file.
// Calling
//   std::ofstream output(outputPath, std::ios::binary)
// or calling
//   std::fstream output(outputPath, std::ios::binary | std::ios::out)
// causes the file to be truncated which can lead to race conditions if called
// in each thread. More information about the truncate behavior is available
// here:
// https://stackoverflow.com/questions/39256916/does-stdofstream-truncate-or-append-by-default#:~:text=It%20truncates%20by%20default
// Using ate or app does not resolve this.

FileStream::FileStream(std::filesystem::path path) : path_(std::move(path)) {
  if (!std::filesystem::exists(path_)) {
    auto s = std::ofstream(path_);
    CHECK(s) << "cannot create " << path_;
  }
}

void FileStream::Resize(std::streamsize size) const {
  std::error_code ec;
  std::filesystem::resize_file(path_, size, ec);
  CHECK(!ec) << ec.message();
}

std::fstream FileStream::GetStream() const {
  auto s = std::fstream(path_, std::ios::binary | std::ios::in | std::ios::out);
  // TODO(kyotov): discuss with @ashish why this is necessary?
  s.exceptions(std::fstream::failbit | std::fstream::badbit);
  return std::move(s);
}

}  // namespace kysync
