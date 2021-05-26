#include "http_reader.h"

#include <glog/logging.h>
#include <httplib.h>

namespace kysync {

static void ReadHttpLine(std::istream &input, std::string &buffer, int max) {
  enum class State {  //
    kNotInTerminatingState,
    kPossiblyTerminating,
    kTerminated
  };

  for (auto state = State::kNotInTerminatingState;
       state != State::kTerminated && buffer.size() < max;)
  {
    char c{};
    CHECK_EQ(input.get(c).gcount(), 1);
    buffer += c;
    switch (state) {
      case State::kNotInTerminatingState:  // not in \r\n
        if (c == '\r') {
          state = State::kPossiblyTerminating;
        }
        break;
      case State::kPossiblyTerminating:  // just after \r
        if (c == '\n') {          // seen \r\n
          state = State::kTerminated;
        } else if (c == '\r') {  // handle \r\r\n
          state = State::kPossiblyTerminating;
        } else {
          state = State::kNotInTerminatingState;
        }
        break;
      case State::kTerminated:
        break;
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

static bool IsMultirangeResponse(httplib::Response &response) {
  return response.get_header_value("Content-Type")
      .starts_with("multipart/byteranges");
}

using ChunkCallback = std::function<
    void(size_t /*beg*/, size_t /*end*/, std::istream & /*data*/)>;

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

  enum class State {  //
    kBoundary,
    kHeader,
    kDataChunk,
    kTerminated
  };

  for (auto state = State::kBoundary; state != State::kTerminated;) {
    switch (state) {
      case State::kBoundary: {
        buffer.clear();
        ReadHttpLine(body, buffer, 2);
        if (buffer == crlf) {
          continue;  // keep skipping \r\n until -- shows up
        }
        CHECK_EQ(buffer, dash);
        CheckPrefixAndAdvance(body, boundary, buffer);
        buffer.clear();
        ReadHttpLine(body, buffer, 2);
        if (buffer == crlf) {
          state = State::kHeader;
        } else {
          CHECK_EQ(buffer, dash);
          state = State::kTerminated;
        }
        break;
      }
      case State::kHeader: {
        buffer.clear();
        ReadHttpLine(body, buffer, 1024);
        if (buffer == crlf) {  // end of headers
          state = State::kDataChunk;
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
      case State::kDataChunk: {
        chunk_callback(beg, end, body);
        state = State::kBoundary;
        break;
      }
      case State::kTerminated:
        break;
    }
  }
}

size_t HttpReader::Read(void *buffer, httplib::Ranges ranges) {
  // TODO(kyotov): maybe make httplib contribution to pass ranges by const ref
  auto range_header = httplib::make_range_header(std::move(ranges));
  auto res = client_->Get(path_.c_str(), {range_header});

  CHECK(res.error() == httplib::Error::Success) << path_;
  CHECK(res->status == 206 || res->status == 200) << path_;

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
  CHECK_NE(pos, url.npos);
  pos = url.find('/', pos + 2);
  CHECK_NE(pos, url.npos);

  host_ = url.substr(0, pos);
  path_ = url.substr(pos);

  client_ = std::make_unique<httplib::Client>(host_.c_str());
  client_->set_keep_alive(true);
}

HttpReader::~HttpReader() = default;

size_t HttpReader::GetSize() const {
  auto res = client_->Head(path_.c_str());

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
    std::vector<BatchRetrivalInfo> &batched_retrieval_infos) {
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