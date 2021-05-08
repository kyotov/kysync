#include "http_reader.h"

#include <glog/logging.h>
#include <httplib.h>

namespace kysync {

class HttpReader::Impl {
  const std::string kHost;
  const std::string kPath;
  httplib::Client cli_;

public:
  explicit Impl(std::string host, std::string path)
      : kHost(std::move(host)),
        kPath(std::move(path)),
        cli_(kHost.c_str()) {
    cli_.set_keep_alive(true);
  }

  [[nodiscard]] size_t GetSize() {
    auto res = cli_.Head(kPath.c_str());

    CHECK(res->has_header("Content-Length"));
    auto size = res->get_header_value("Content-Length");

    return strtoull(size.c_str(), nullptr, 10);
  }

  size_t Read(void *buffer, httplib::Ranges ranges) {
    auto range_header = httplib::make_range_header(ranges);
    auto res = cli_.Get(kPath.c_str(), {range_header});

    CHECK_EQ(res.error(), httplib::Success);
    CHECK_EQ(res->status, 206);

    auto count = res->body.size();
    memcpy(buffer, res->body.data(), count);
    return count;
  }

  size_t Read(void *buffer, size_t offset, size_t size) {
    auto beg_offset = offset;
    auto end_offset = offset + size - 1;
    return Read(buffer, {{beg_offset, end_offset}});
  }

  size_t Read(
      void *buffer,
      std::vector<BatchedRetrivalInfo> &batched_retrievals_info) {
    httplib::Ranges ranges;
    for (auto &retrieval_info : batched_retrievals_info) {
      auto end_offset =
          retrieval_info.source_begin_offset + retrieval_info.size_to_read - 1;
      ranges.push_back({retrieval_info.source_begin_offset, end_offset});
    }
    return Read(buffer, ranges);
  }
};

HttpReader::HttpReader(const std::string &url) {
  auto pos = url.find("//");
  CHECK(pos != url.npos);
  pos = url.find('/', pos + 2);
  CHECK(pos != url.npos);

  impl_ = std::make_unique<Impl>(url.substr(0, pos), url.substr(pos));
}

HttpReader::~HttpReader() = default;

size_t HttpReader::GetSize() const { return impl_->GetSize(); }

size_t HttpReader::Read(void *buffer, size_t offset, size_t size) const {
  auto count = impl_->Read(buffer, offset, size);
  return Reader::Read(buffer, offset, count);
}

size_t HttpReader::Read(
    void *buffer,
    std::vector<BatchedRetrivalInfo> &batched_retrievals_info) const {
  auto count = impl_->Read(buffer, batched_retrievals_info);
  return Reader::Read(nullptr, 0, count);
}

}  // namespace kysync