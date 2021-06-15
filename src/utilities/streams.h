#ifndef KSYNC_STREAMS_H
#define KSYNC_STREAMS_H

#include <glog/logging.h>

#include <filesystem>
#include <fstream>
#include <limits>
#include <ostream>
#include <vector>

namespace kysync {

std::fstream GetOutputStream(
    const std::filesystem::path &path,
    std::streamoff offset);

std::streamsize
StreamWrite(std::ostream &stream, const void *data, std::streamsize size);

template <typename T>
std::streamsize StreamWrite(
    std::ostream &stream,
    const std::vector<T> &data,
    std::streamsize max_size_to_write) {
  CHECK_GE(max_size_to_write, 0);

  auto size = static_cast<std::streamsize>(data.size() * sizeof(T));
  auto size_to_write = std::min(size, max_size_to_write);

  return StreamWrite(stream, data.data(), size_to_write);
}

template <typename T>
std::streamsize StreamWrite(std::ostream &stream, const std::vector<T> &data) {
  return StreamWrite(stream, data, std::numeric_limits<std::streamsize>::max());
}

}  // namespace kysync

#endif  // KSYNC_STREAMS_H
