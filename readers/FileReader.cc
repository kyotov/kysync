#include "FileReader.h"

#include <fstream>
#include <istream>

namespace fs = std::filesystem;

struct FileReader::Impl {
  const fs::path path_;
  std::ifstream data_;

  explicit Impl(fs::path _path) : path_(std::move(_path)) {
    data_.open(path_, std::ios::binary);
  }

  [[nodiscard]] size_t GetSize() const { return fs::file_size(path_); }

  size_t Read(void *buffer, size_t offset, size_t size) {
    // the seekg was failing if the eof bit was set... :(
    // according to https://devdocs.io/cpp/io/basic_istream/seekg this should
    // not happen since C++11, which is supposed to clear the eof bit, but alas
    data_.clear();

    data_.seekg(offset);
    data_.read((char *)buffer, size);
    return data_.gcount();
  }
};

FileReader::FileReader(const std::filesystem::path &path)
    : impl_(std::make_unique<Impl>(path)) {}

FileReader::~FileReader() = default;

size_t FileReader::GetSize() const { return impl_->GetSize(); }

size_t FileReader::Read(void *buffer, size_t offset, size_t size) const {
  auto count = impl_->Read(buffer, offset, size);
  return Reader::Read(buffer, offset, count);
}
