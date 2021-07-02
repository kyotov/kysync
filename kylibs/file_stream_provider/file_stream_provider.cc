#include <glog/logging.h>
#include <ky/file_stream_provider.h>

#include <fstream>

namespace ky {

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

FileStreamProvider::FileStreamProvider(std::filesystem::path path)
    : path_(std::move(path)) {
  if (!std::filesystem::exists(path_)) {
    auto s = std::ofstream(path_);
    CHECK(s) << "cannot create " << path_;
  }
}

void FileStreamProvider::Resize(std::streamsize size) const {
  std::error_code ec;
  std::filesystem::resize_file(path_, size, ec);
  CHECK(!ec) << ec.message();
}

std::fstream FileStreamProvider::CreateFileStream() const {
  // Note: the following is unable to use braced initializers because of the way
  //       the MSVC library is implemented... the corresponding constructor is
  //       explicit, and thus the braced initializer is not allowed.
  // NOLINTNEXTLINE(modernize-return-braced-init-list)
  return std::fstream(path_, std::ios::binary | std::ios::in | std::ios::out);
}

}  // namespace ky
