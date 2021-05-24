#include "http_reader.h"

#include <glog/logging.h>
#include <httplib.h>

namespace kysync {

static void ReadHttpLine(std::istream &input, std::string &buffer, int max) {
  enum states { NOT_IN_TERMINATING_STATE, POSSIBLY_TERMINATING, TERMINATED };
  for (auto state = NOT_IN_TERMINATING_STATE;
       state != TERMINATED && buffer.size() < max;)
  {
    char c;
    CHECK_EQ(input.get(c).gcount(), 1);
    buffer += c;
    switch (state) {
      case NOT_IN_TERMINATING_STATE:  // not in \r\n
        if (c == '\r') {
          state = POSSIBLY_TERMINATING;
        }
        break;
      case POSSIBLY_TERMINATING:  // just after \r
        if (c == '\n') {          // seen \r\n
          state = TERMINATED;
        } else if (c == '\r') {  // handle \r\r\n
          state = POSSIBLY_TERMINATING;
        } else {
          state = NOT_IN_TERMINATING_STATE;
        }
        break;
      default:
        LOG_ASSERT(false) << " Unexpected internal state: " << state;
    }
  }
}

static void CheckPrefixAndAdvance(
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

static bool IsMultirangeResponse(httplib::Response &response) {
  return response.get_header_value("Content-Type")
      .starts_with("multipart/byteranges");
}

static void ParseMultipartByterangesResponse(
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
  enum { BOUNDARY, HEADER, DATA_CHUNK, TERMINATED };
  for (auto state = BOUNDARY; state != TERMINATED;) {
    switch (state) {
      case BOUNDARY: {
        buffer.clear();
        ReadHttpLine(body, buffer, 2);
        if (buffer == crlf) {
          continue;
        }
        CHECK_EQ(buffer, dash);
        //          CheckPrefixAndAdvance(body, crlf, buffer);
        //          CheckPrefixAndAdvance(body, dash, buffer);
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
        state = BOUNDARY;
        break;
      }
      default:
        LOG_ASSERT(false)
            << " Unexpected state when parsing multipart response:  " << state;
    }
  }
}

size_t HttpReader::Read(void *buffer, httplib::Ranges ranges) {
  auto range_header = httplib::make_range_header(ranges);
  auto res = cli_->Get(kPath.c_str(), {range_header});

  CHECK(res.error() == httplib::Error::Success);
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
  } else {
    count = res->body.size();
    memcpy(buffer, res->body.data(), count);
  }
  return count;
}

HttpReader::HttpReader(const std::string &url) {
  auto pos = url.find("//");
  CHECK(pos != url.npos);
  pos = url.find('/', pos + 2);
  CHECK(pos != url.npos);

  kHost = url.substr(0, pos);
  kPath = url.substr(pos);
  cli_ = std::make_unique<httplib::Client>(kHost.c_str());
  cli_->set_keep_alive(true);
}

HttpReader::~HttpReader() = default;

size_t HttpReader::GetSize() const {
  auto res = cli_->Head(kPath.c_str());

  CHECK(res->has_header("Content-Length"));
  auto size = res->get_header_value("Content-Length");

  return std::stoull(size, nullptr, 10);
}

size_t HttpReader::Read(void *buffer, size_t offset, size_t size) {
  auto beg_offset = offset;
  auto end_offset = offset + size - 1;
  auto count = Read(buffer, {{beg_offset, end_offset}});
  return Reader::Read(buffer, offset, count);
}

size_t HttpReader::Read(
    void *buffer,
    std::vector<BatchedRetrivalInfo> &batched_retrieval_infos) {
  httplib::Ranges ranges;
  for (auto &retrieval_info : batched_retrieval_infos) {
    auto end_offset =
        retrieval_info.source_begin_offset + retrieval_info.size_to_read - 1;
    ranges.push_back({retrieval_info.source_begin_offset, end_offset});
  }
  auto count = Read(buffer, ranges);
  return Reader::Read(nullptr, 0, count);
}

}  // namespace kysync