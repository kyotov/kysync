#include "Reader.h"

#include <glog/logging.h>

#include <filesystem>

#include "FileReader.h"
#include "HttpReader.h"
#include "MemoryReader.h"

namespace fs = std::filesystem;

class Reader::Impl {
public:
  Metric total_reads_;
  Metric total_bytes_read_;

  void Accept(MetricVisitor &visitor) const {
    VISIT(visitor, total_reads_);
    VISIT(visitor, total_bytes_read_);
  }
};

Reader::Reader() : impl_(std::make_unique<Impl>()) {}

Reader::~Reader() = default;

size_t Reader::Read(void * /*buffer*/, size_t /*offset*/, size_t size) const {
  impl_->total_reads_++;
  impl_->total_bytes_read_ += size;
  return size;
}

void Reader::Accept(MetricVisitor &visitor) const { impl_->Accept(visitor); }

std::unique_ptr<Reader> Reader::Create(const std::string &uri) {
  if (uri.starts_with("http://") || uri.starts_with("https://")) {
    return std::make_unique<HttpReader>(uri);
  }

  std::string file = "file://";
  if (uri.starts_with(file)) {
    auto path = uri.substr(file.size());

    if (!fs::exists(path)) {
      LOG(ERROR) << "path '" << path << "' not found";
      throw std::invalid_argument(uri);
    }

    return std::make_unique<FileReader>(path);
  }

  std::string memory = "memory://";
  if (uri.starts_with(memory)) {
    auto s = uri.substr(memory.size());
    char *current;

    // should this be necessary!? at some point it just started failing without
    errno = 0;

    auto buffer = (void *)strtoull(s.c_str(), &current, 16);
    if (errno != 0 || *current != ':') {
      LOG(ERROR) << "errno=" << errno << " current=" << (int)*current
                 << " uri=" << uri;
      throw std::invalid_argument(uri);
    }

    auto size = strtoull(++current, &current, 16);
    if (errno != 0 || *current != 0) {
      LOG(ERROR) << "errno=" << errno << " current=" << (int)*current
                 << " uri=" << uri;
      throw std::invalid_argument(uri);
    }

    return std::make_unique<MemoryReader>(buffer, size);
  }

  LOG(ERROR) << "unknown protocol for uri=" << uri;
  throw std::invalid_argument(uri);
}
