#include <glog/logging.h>
#include <kysync/readers/file_reader.h>
#include <kysync/readers/http_reader.h>
#include <kysync/readers/memory_reader.h>
#include <kysync/readers/reader.h>

#include <filesystem>

namespace kysync {

namespace fs = std::filesystem;

std::streamsize Reader::Read(
    void * /*buffer*/,
    std::streamoff /*offset*/,
    std::streamsize size) {
  total_reads_++;
  total_bytes_read_ += size;
  return size;
}

std::streamsize Reader::Read(
    std::vector<BatchRetrivalInfo> &batch_retrieval_infos,
    const ReadCallback &read_callback) {
  std::streamsize total_size_read = 0;
  std::vector<char> buffer;
  for (const auto &retrieval_info : batch_retrieval_infos) {
    if (retrieval_info.size_to_read > buffer.size()) {
      buffer.resize(retrieval_info.size_to_read);
    }
    auto size_read = Read(
        buffer.data(),
        retrieval_info.source_begin_offset,
        retrieval_info.size_to_read);
    auto end_offset =
        retrieval_info.source_begin_offset + retrieval_info.size_to_read - 1;
    read_callback(
        retrieval_info.source_begin_offset,
        end_offset,
        buffer.data());
    total_size_read += size_read;
  }
  return total_size_read;
}

void Reader::Accept(ky::metrics::MetricVisitor &visitor) {
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

    // TODO(kyotov): review this pointer arithmetic business...
    //  there may be a better way to implement this

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    char *current = nullptr;

    // should this be necessary!? at some point it just started failing without
    errno = 0;

    // NOLINTNEXTLINE(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast)
    auto *buffer = reinterpret_cast<void *>(strtoull(s.c_str(), &current, 16));
    if (errno != 0 || *current != ':') {
      LOG(ERROR) << "errno=" << errno
                 << " current=" << static_cast<int>(*current) << " uri=" << uri;
      throw std::invalid_argument(uri);
    }

    auto size = strtoull(++current, &current, 16);
    if (errno != 0 || *current != 0) {
      LOG(ERROR) << "errno=" << errno
                 << " current=" << static_cast<int>(*current) << " uri=" << uri;
      throw std::invalid_argument(uri);
    }

    return std::make_unique<MemoryReader>(buffer, size);
  }

  LOG(ERROR) << "unknown protocol for uri=" << uri;
  throw std::invalid_argument(uri);
}

}  // namespace kysync