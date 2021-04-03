#include "HttpReader.h"

#include <glog/logging.h>
#include <httplib.h>

struct HttpReader::Impl {
  const std::string host_;
  const std::string path_;
  httplib::Client cli_;

  explicit Impl(std::string host, std::string path)
      : host_(std::move(host)),
        path_(std::move(path)),
        cli_(host_.c_str()) {}

  [[nodiscard]] size_t GetSize() {
    auto res = cli_.Head(path_.c_str());

    CHECK(res->has_header("Content-Length"));
    auto size = res->get_header_value("Content-Length");

    return strtoull(size.c_str(), nullptr, 10);
  }

  size_t Read(void *buffer, size_t offset, size_t size) {
    auto beg_offset = offset;
    auto end_offset = offset + size - 1;

    auto range = httplib::make_range_header({{beg_offset, end_offset}});
    auto res = cli_.Get(path_.c_str(), {range});

    CHECK_EQ(res.error(), httplib::Success);
    CHECK_EQ(res->status, 206) << beg_offset << "-" << end_offset;

    auto count = res->body.size();
    memcpy(buffer, res->body.data(), count);

    return count;
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
