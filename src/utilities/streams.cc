#include "streams.h"

#include <glog/logging.h>

#include <filesystem>
#include <random>

#include "../checksums/strong_checksum.h"

namespace kysync {

std::fstream GetOutputStream(
    const std::filesystem::path &path,
    std::streamoff offset) {
  if (!std::filesystem::exists(path)) {
    std::ofstream(path, std::ios::binary);
  }
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
  std::fstream output(
      path,
      std::ios::binary | std::fstream::out | std::fstream::in);
  // TODO(kyotov): discuss with ashish on why this was needed?
  output.exceptions(std::fstream::failbit | std::fstream::badbit);
  // TODO(kyotov): discuss with ashish on why this is seeking and not the client
  output.seekp(offset);
  return std::move(output);
}

std::streamsize
StreamWrite(std::ostream &stream, const void *data, std::streamsize size) {
  CHECK_GE(size, 0);
  stream.write(static_cast<const char *>(data), size);
  CHECK(stream);
  return size;
}

}  // namespace kysync
