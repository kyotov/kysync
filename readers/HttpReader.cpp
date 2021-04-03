#include "HttpReader.h"

#include <glog/logging.h>
#include <httplib.h>

struct HttpReader::Impl {
  const std::string host;
  const std::string path;
  httplib::Client cli;

  explicit Impl(std::string _host, std::string _path)
      : host(std::move(_host)),
        path(std::move(_path)),
        cli(host.c_str()) {}

  [[nodiscard]] size_t size() {
    auto res = cli.Head(path.c_str());

    CHECK(res->has_header("Content-Length"));
    auto size = res->get_header_value("Content-Length");

    return strtoull(size.c_str(), nullptr, 10);
  }

  size_t read(void *buffer, size_t offset, size_t size) {
    auto begOffset = offset;
    auto endOffset = offset + size - 1;

    auto range = httplib::make_range_header({{begOffset, endOffset}});
    auto res = cli.Get(path.c_str(), {range});

    CHECK_EQ(res.error(), httplib::Success);
    CHECK_EQ(res->status, 206) << begOffset << "-" << endOffset;

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

  pImpl = std::make_unique<Impl>(url.substr(0, pos), url.substr(pos));
}

HttpReader::~HttpReader() = default;

size_t HttpReader::size() const { return pImpl->size(); }

size_t HttpReader::read(void *buffer, size_t offset, size_t size) const {
  auto count = pImpl->read(buffer, offset, size);
  return Reader::read(buffer, offset, count);
}
