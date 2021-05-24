#include "reader.h"

#include <glog/logging.h>

#include <filesystem>

#include "file_reader.h"
#include "http_reader.h"
#include "memory_reader.h"

namespace kysync {

namespace fs = std::filesystem;

size_t Reader::Read(void * /*buffer*/, size_t /*offset*/, size_t size) {
  total_reads_++;
  total_bytes_read_ += size;
  return size;
}

size_t Reader::Read(
    void *buffer,
    std::vector<BatchedRetrivalInfo> &batched_retrieval_infos) {
  size_t size_read = 0;
  for (auto &retrieval_info : batched_retrieval_infos) {
    size_read += Read(
        static_cast<char *>(buffer) + size_read,
        retrieval_info.source_begin_offset,
        retrieval_info.size_to_read);
  }
  return size_read;
}

void Reader::Accept(MetricVisitor &visitor) {
  VISIT_METRICS(total_reads_);
  VISIT_METRICS(total_bytes_read_);
}

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

}  // namespace kysync