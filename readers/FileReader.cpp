#include "FileReader.h"

#include <fstream>

namespace fs = std::filesystem;

struct FileReader::Impl {
  const fs::path path;
  std::ifstream data;

  explicit Impl(fs::path _path) : path(std::move(_path))
  {
    data.open(path, std::ios::binary);
  }

  [[nodiscard]] size_t size() const
  {
    return fs::file_size(path);
  }

  size_t read(void *buffer, size_t offset, size_t size)
  {
    // the seekg was failing if the eof bit was set... :(
    // according to https://devdocs.io/cpp/io/basic_istream/seekg this should
    // not happen since C++11, which is supposed to clear the eof bit, but alas
    data.clear();

    data.seekg(offset);
    data.read((char *)buffer, size);
    return data.gcount();
  }
};

FileReader::FileReader(const std::filesystem::path &path)
    : pImpl(std::make_unique<Impl>(path))
{
}

FileReader::~FileReader() = default;

size_t FileReader::size() const
{
  return pImpl->size();
}

size_t FileReader::read(void *buffer, size_t offset, size_t size) const
{
  auto count = pImpl->read(buffer, offset, size);
  return Reader::read(buffer, offset, count);
}
