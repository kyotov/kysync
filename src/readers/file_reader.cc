#include "file_reader.h"

#include <fstream>

namespace kysync {

namespace fs = std::filesystem;

class FileReader::Impl {
  const fs::path kPath;
  std::ifstream data_;

public:
  explicit Impl(fs::path path) : kPath(std::move(path)) {
    data_.open(kPath, std::ios::binary);
  }

  [[nodiscard]] size_t GetSize() const { return fs::file_size(kPath); }

  auto Read(void *buffer, size_t offset, size_t size) {
    // the seekg was failing if the eof bit was set... :(
    // according to https://devdocs.io/cpp/io/basic_istream/seekg this should
    // not happen since C++11, which is supposed to clear the eof bit, but alas
    data_.clear();

    data_.seekg(offset);
    data_.read(static_cast<char *>(buffer), size);
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

}  // namespace kysync