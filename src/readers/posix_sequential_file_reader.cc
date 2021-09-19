#include <fcntl.h>
#include <sys/mman.h>
#include <glog/logging.h>
#include <kysync/readers/posix_sequential_file_reader.h>

namespace kysync {

namespace fs = std::filesystem;

PosixSequentialFileReader::PosixSequentialFileReader(const std::filesystem::path &path)
    : path_(path),
      cached_current_offset_(0) {

  fd_ = open(path.c_str(), O_RDONLY);
  CHECK(fd_ >= 0) << "unable to open " << path << " for reading";

  posix_fadvise(fd_, 0, 0, POSIX_FADV_SEQUENTIAL);
}

PosixSequentialFileReader::~PosixSequentialFileReader() {
  close(fd_);
}

std::streamsize PosixSequentialFileReader::GetSize() const {
  // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
  return fs::file_size(path_);
}

std::streamsize
PosixSequentialFileReader::Read(void *buffer, std::streamoff offset, std::streamsize size) {
  if (cached_current_offset_ != offset) {
    lseek(fd_, offset, SEEK_SET);
    cached_current_offset_ = offset;
  }
  int count = read(fd_, buffer, size);
  cached_current_offset_ += count;

  return Reader::Read(buffer, offset, count);
}

}  // namespace kysync