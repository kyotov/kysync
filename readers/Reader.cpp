#include "Reader.h"

#include <glog/logging.h>

#include <filesystem>
#include <sstream>

#include "FileReader.h"
#include "MemoryReader.h"

namespace fs = std::filesystem;

struct Reader::Impl {
  Metric totalReads;
  Metric totalBytesRead;

  void accept(MetricVisitor &visitor)
  {
    VISIT(visitor, totalReads);
    VISIT(visitor, totalBytesRead);
  }
};

Reader::Reader() : pImpl(std::make_unique<Impl>()) {}

Reader::~Reader() = default;

size_t Reader::read(void *buffer, size_t offset, size_t size) const
{
  pImpl->totalReads++;
  pImpl->totalBytesRead += size;
  return size;
}

void Reader::accept(MetricVisitor &visitor) const
{
  pImpl->accept(visitor);
}

std::unique_ptr<Reader> Reader::create(const std::string &uri)
{
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
