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

  void ReadHttpLine(std::istream &input, std::string &buffer, int max) {
    enum states { NOT_IN_TERMINATING_STATE, TERMINATING, TERMINATED };
    for (auto state = NOT_IN_TERMINATING_STATE;
         state != TERMINATED && buffer.size() < max;)
    {
      char c;
      CHECK_EQ(input.get(c).gcount(), 1);
      buffer += c;
      switch (state) {
        case NOT_IN_TERMINATING_STATE:  // not in \r\n
          if (c == '\r') {
            state = TERMINATING;
          }
          break;
        case TERMINATING:   // just after \r
          if (c == '\n') {  // seen \r\n
            state = TERMINATED;
          } else if (c == '\r') {  // handle \r\r\n
            state = TERMINATING;
          } else {
            state = NOT_IN_TERMINATING_STATE;
          }
          break;
        default:
          LOG_ASSERT("Unexpected internal state: ") << state;
      }
    }
  }

  void CheckPrefixAndAdvance(
      std::istream &input,
      const std::string &prefix,
      std::string &buffer) {
    buffer.resize(prefix.size(), ' ');
    input.read(buffer.data(), prefix.size());
    CHECK_EQ(input.gcount(), prefix.size());
    CHECK(std::equal(prefix.begin(), prefix.end(), buffer.begin()));
  }

  using ChunkCallback = std::function<
      void(size_t /*beg*/, size_t /*end*/, std::istream & /*data*/)>;

  void ParseMultipartByterangesResponse(
      httplib::Response &response,
      ChunkCallback chunk_callback) {
    auto content_type = response.get_header_value("Content-Type");
    LOG_ASSERT(IsMultirangeResponse(response));
    std::string boundary;
    CHECK(httplib::detail::parse_multipart_boundary(content_type, boundary));

    auto dash = std::string("--");
    auto crlf = std::string("\r\n");
    auto buffer = std::string();
    auto body = std::stringstream(response.body);

    size_t beg = 0;
    size_t end = 0;
    enum states { INITIAL, BOUNDARY, HEADER, DATA_CHUNK, TERMINATED };
    for (auto state = INITIAL; state != TERMINATED;) {
      switch (state) {
        case INITIAL: {
          // There can be an optional CRLF before the boundary
          auto position = body.tellg();
          ReadHttpLine(body, buffer, 2);
          if (buffer == crlf) {
            // No-op
          } else {
            body.seekg(position);
          }
          state = BOUNDARY;
          break;
        }
        case BOUNDARY: {
          CheckPrefixAndAdvance(body, dash, buffer);
          CheckPrefixAndAdvance(body, boundary, buffer);
          buffer.clear();
          ReadHttpLine(body, buffer, 2);
          if (buffer == crlf) {
            state = HEADER;
          } else {
            CHECK_EQ(buffer, dash);
            state = TERMINATED;
          }
          break;
        }
        case HEADER: {
          buffer.clear();
          ReadHttpLine(body, buffer, 1024);
          if (buffer == crlf) {  // end of headers
            state = DATA_CHUNK;
            break;  // continue with data chunk
          }
          auto re = std::regex(
              R"(^Content-Range: bytes (\d+)-(\d+)/(\d+)\r\n$)",
              std::regex_constants::icase);
          auto m = std::smatch();
          if (std::regex_match(buffer, m, re)) {
            beg = stoul(buffer.substr(m.position(1), m.length(1)), nullptr, 10);
            end = stoul(buffer.substr(m.position(2), m.length(2)), nullptr, 10);
          }
          break;  // continue with headers
        }
        case DATA_CHUNK: {
          chunk_callback(beg, end, body);
          CheckPrefixAndAdvance(body, crlf, buffer);
          state = BOUNDARY;
          break;
        }
        default:
          LOG_ASSERT("Unexpected state when parsing multipart response: ")
              << state;
      }
    }
  }

  bool IsMultirangeResponse(httplib::Response &response) {
    return response.get_header_value("Content-Type")
        .starts_with("multipart/byteranges");
  }

  size_t Read(void *buffer, httplib::Ranges ranges) {
    auto range_header = httplib::make_range_header(ranges);
    auto res = cli_.Get(kPath.c_str(), {range_header});

    CHECK_EQ(res.error(), httplib::Success);
    CHECK(res->status == 206 || res->status == 200);

    size_t count = 0;
    if (IsMultirangeResponse(res.value())) {
      // Note this expects data to be provided in order of range request made
      ParseMultipartByterangesResponse(
          res.value(),
          [&buffer, &count](size_t beg, size_t end, std::istream &s) {
            auto count_in_chunk = end - beg + 1;
            s.read(static_cast<char *>(buffer) + count, count_in_chunk);
            count += count_in_chunk;
          });
      LOG(INFO) << "Parsing multirange response";
    } else {
      count = res->body.size();
      memcpy(buffer, res->body.data(), count);
    }
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