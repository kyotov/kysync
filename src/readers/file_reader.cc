#include "file_reader.h"

#include <fstream>

namespace kysync {

namespace fs = std::filesystem;

FileReader::FileReader(std::filesystem::path path) : kPath(std::move(path)) {
  data_.open(kPath, std::ios::binary);
}

size_t FileReader::GetSize() const { return fs::file_size(kPath); }

size_t FileReader::Read(void *buffer, size_t offset, size_t size) {
  // the seekg was failing if the eof bit was set... :(
  // according to https://devdocs.io/cpp/io/basic_istream/seekg this should
  // not happen since C++11, which is supposed to clear the eof bit, but alas
  data_.clear();

  data_.seekg(offset);
  data_.read(static_cast<char *>(buffer), size);
  return Reader::Read(buffer, offset, data_.gcount());
}

}  // namespace kysync