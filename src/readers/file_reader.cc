#include "file_reader.h"

#include <glog/logging.h>

namespace kysync {

namespace fs = std::filesystem;

FileReader::FileReader(const std::filesystem::path &path)
    : path_(path),
      data_(std::ifstream(path, std::ios::binary)) {
  CHECK(data_) << "unable to open " << path << " for reading";
}

std::streamsize FileReader::GetSize() const {
  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  return fs::file_size(path_);
}

std::streamsize
FileReader::Read(void *buffer, std::streamoff offset, std::streamsize size) {
  // the seekg was failing if the eof bit was set... :(
  // according to https://devdocs.io/cpp/io/basic_istream/seekg this should
  // not happen since C++11, which is supposed to clear the eof bit, but alas
  data_.clear();

  data_.seekg(offset);
  data_.read(static_cast<char *>(buffer), size);
  return Reader::Read(buffer, offset, data_.gcount());
}

}  // namespace kysync