#include <glog/logging.h>
#include <gtest/gtest.h>
#include <httplib.h>
#include <ky/temp_path.h>
#include <kysync/path_config.h>
#include <kysync/readers/batch_retrieval_info.h>
#include <kysync/readers/reader.h>
#include <kysync/test_http_servers/http_server.h>

#include <array>
#include <fstream>
#include <sstream>

#include "fixture.h"

namespace kysync {

namespace fs = std::filesystem;

class HttpTests : public Fixture {};

void WriteFile(const fs::path& path, const std::string& data) {
  std::ofstream f(path);
  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  f.write(data.c_str(), data.size());
}

template <typename Client>
void CheckRangeGet(Client& client, const std::string& data, int beg, int end) {
  auto expected_result =
      end == -1 ? data.substr(beg) : data.substr(beg, end - beg + 1);

  auto range = httplib::make_range_header({{beg, end}});
  auto response = client.Get("/test.data", {range});
  EXPECT_TRUE(response.error() == httplib::Error::Success);
  EXPECT_EQ(response->body, expected_result);
}

static void CheckPrefixAndAdvance(
    std::istream& input,
    const std::string& prefix,
    std::string& buffer) {
  buffer.resize(prefix.size(), ' ');
  // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
  input.read(buffer.data(), prefix.size());
  CHECK_EQ(input.gcount(), prefix.size());
  CHECK(std::equal(prefix.begin(), prefix.end(), buffer.begin()));
}

static void ReadLine(std::istream& input, std::string& buffer, int max) {
  for (auto state = 0; state >= 0 && buffer.size() < max;) {
    char c = 0;
    CHECK_EQ(input.get(c).gcount(), 1);
    buffer += c;
    switch (state) {
      case 0:  // not in \r\n
        if (c == '\r') {
          state = 1;
        }
        break;
      case 1:  // just after \r
        if (c == '\n') {
          state = -1;
        } else {
          state = 0;
        }
        break;
      default:
        CHECK(false) << "invalid state";
    }
  }
}

using ChunkCallback = std::function<void(
    std::streamoff /*beg*/,
    std::streamoff /*end*/,
    std::istream& /*data*/)>;

static void ParseMultipartByterangesResponse(
    const httplib::Response& response,
    const ChunkCallback& chunk_callback) {
  auto content_type = response.get_header_value("Content-Type");
  CHECK(content_type.starts_with("multipart/byteranges"));

  std::string boundary;
  CHECK(httplib::detail::parse_multipart_boundary(content_type, boundary));

  auto dash = std::string("--");
  auto crlf = std::string("\r\n");
  auto buffer = std::string();

  auto body = std::stringstream(response.body);

  std::streamoff beg = 0;
  std::streamoff end = 0;

  for (auto state = 0; state >= 0;) {
    switch (state) {
      case 0: {  // boundary
        CheckPrefixAndAdvance(body, dash, buffer);
        CheckPrefixAndAdvance(body, boundary, buffer);

        buffer.clear();
        ReadLine(body, buffer, 2);

        if (buffer == crlf) {
          state = 1;
        } else {
          CHECK_EQ(buffer, dash);
          state = -1;
        }

        break;
      }

      case 1: {  // headers
        buffer.clear();
        ReadLine(body, buffer, 1024);

        if (buffer == crlf) {  // end of headers
          state = 2;
          break;  // continue with data chunk
        }

        auto re = std::regex(R"(^Content-Range: bytes (\d+)-(\d+)/(\d+)\r\n$)");
        auto m = std::smatch();

        if (std::regex_match(buffer, m, re)) {
          beg = stoul(buffer.substr(m.position(1), m.length(1)), nullptr, 10);
          end = stoul(buffer.substr(m.position(2), m.length(2)), nullptr, 10);
        }

        break;  // continue with headers
      }

      case 2: {  // data chunk
        chunk_callback(beg, end, body);
        CheckPrefixAndAdvance(body, crlf, buffer);
        state = 0;
        break;  // back to boundary
      }

      default:
        CHECK(false) << "invalid state";
    }
  }
}

template <typename Client>
void CheckGet(const fs::path& path, Client& client) {
  std::string data = "0123456789";
  WriteFile(path / "test.data", data);

  auto response = client.Get("/test.data");
  EXPECT_TRUE(response.error() == httplib::Error::Success);
  EXPECT_EQ(response->body, data);

  CheckRangeGet(client, data, 0, 0);
  CheckRangeGet(client, data, 0, 1024);
  CheckRangeGet(client, data, 2, 1024);
  CheckRangeGet(client, data, 2, 4);
  CheckRangeGet(client, data, 0, data.size());
  CheckRangeGet(client, data, 0, -1);
  CheckRangeGet(client, data, 5, -1);

  {
    auto range = httplib::make_range_header({{1, 3}, {5, 7}, {9, -1}});
    auto result = client.Get("/test.data", {range});
    EXPECT_TRUE(result.error() == httplib::Error::Success);
    ParseMultipartByterangesResponse(
        result.value(),
        [&](std::streamoff beg, std::streamoff end, std::istream& s) {
          auto len = end - beg + 1;
          std::vector<char> buffer(len);
          s.read(buffer.data(), len);
          auto chunk = data.substr(beg, len);
          EXPECT_TRUE(std::equal(chunk.begin(), chunk.end(), buffer.begin()));
        });
  }
}

TEST_F(HttpTests, HttpsServer) {  // NOLINT
  auto tmp = ky::TempPath();
  auto cert_path = fs::path(
      GetEnv("TEST_DATA_DIR", (CMAKE_SOURCE_DIR / "test_data").string()));
  auto server = HttpServer(cert_path, tmp.GetPath(), true);
  auto client = httplib::SSLClient("localhost", server.GetPort());
  client.enable_server_certificate_verification(false);

  CheckGet(tmp.GetPath(), client);
  server.Stop();
}

TEST_F(HttpTests, HttpServer) {  // NOLINT
  auto tmp = ky::TempPath();
  auto server = HttpServer(tmp.GetPath(), true);
  auto client = httplib::Client("localhost", server.GetPort());

  CheckGet(tmp.GetPath(), client);
  server.Stop();
}

// Note: This function and the following 2 tests are temporary and will be
// removed after http_tests.cc have been pushed
void HttpClientMultiRangeTest(
    httplib::Ranges ranges,
    std::streamsize expected_body_size) {
  auto server = HttpServer(CMAKE_SOURCE_DIR / "test_data", true);
  auto range_header = httplib::make_range_header(std::move(ranges));
  httplib::Client http_client("localhost", server.GetPort());
  std::string path("/ubuntu-20.04.2.0-desktop-amd64.list");
  auto res = http_client.Get(path.c_str(), {range_header});
  CHECK(res.error() == httplib::Error::Success);
  CHECK_EQ(res->status, 206);
  LOG(INFO) << "Got body: " << res->body;
  EXPECT_EQ(res->body.size(), expected_body_size);
}

TEST_F(HttpTests, HttpLibMultirangeContiguous) {  // NOLINT
  auto block_size = 4;
  httplib::Ranges ranges;
  ranges.push_back({0, block_size - 1});
  ranges.push_back({block_size, block_size * 2 - 1});
  std::string expected_string("/isolinu");
  HttpClientMultiRangeTest(ranges, 229);
}

TEST_F(HttpTests, HttpLibMultirangeNoncontiguous) {  // NOLINT
  auto block_size = 4;
  httplib::Ranges ranges;
  ranges.push_back({0, block_size - 1});
  ranges.push_back({block_size + 1, block_size * 2});
  std::string expected_string("/isoinux");
  HttpClientMultiRangeTest(ranges, 229);
}

// Note: This function and the following 2 tests are temporary and will be
// removed after http_tests.cc have been pushed
void HttpReaderMultirangeTest(
    std::vector<BatchRetrivalInfo>& batched_retrieval_infos,
    std::string& expected_string) {
  auto server = HttpServer(CMAKE_SOURCE_DIR / "test_data", true);
  std::string uri(
      "http://localhost:" + std::to_string(server.GetPort()) +
      "/ubuntu-20.04.2.0-desktop-amd64.list");
  auto reader = Reader::Create(uri);
  std::streamsize total_size = 0;
  for (auto& retrieval_info : batched_retrieval_infos) {
    total_size += retrieval_info.size_to_read;
  }
  auto buffer = std::vector<char>(total_size);
  auto size_read = reader->Read(buffer.data(), batched_retrieval_infos);
  EXPECT_EQ(size_read, expected_string.size());
  EXPECT_TRUE(
      std::memcmp(
          buffer.data(),
          expected_string.c_str(),
          expected_string.size()) == 0);
}

TEST_F(HttpTests, HttpReaderMultirangeContiguous) {  // NOLINT
  std::vector<BatchRetrivalInfo> batched_retrieval_infos;
  std::streamsize block_size = 4;
  batched_retrieval_infos.push_back(
      {.block_index = 0,
       .source_begin_offset = 0,
       .size_to_read = block_size,
       .offset_to_write_to = 0});
  batched_retrieval_infos.push_back(
      {.block_index = 1,
       .source_begin_offset = block_size,
       .size_to_read = block_size,
       .offset_to_write_to = block_size});
  std::string expected_string("/isolinu");
  HttpReaderMultirangeTest(batched_retrieval_infos, expected_string);
}

TEST_F(HttpTests, HttpReaderMultirangeNoncontiguous) {  // NOLINT
  std::vector<BatchRetrivalInfo> batched_retrieval_infos;
  std::streamsize block_size = 4;
  batched_retrieval_infos.push_back(
      {.block_index = 0,
       .source_begin_offset = 0,
       .size_to_read = block_size,
       .offset_to_write_to = 0});
  batched_retrieval_infos.push_back(
      {.block_index = 1,
       .source_begin_offset = block_size + 1,
       .size_to_read = block_size,
       .offset_to_write_to = block_size});
  batched_retrieval_infos.push_back(
      {.block_index = 1,
       .source_begin_offset = block_size * 2 + 2,
       .size_to_read = block_size,
       .offset_to_write_to = block_size * 2});
  std::string expected_string("/isoinuxadtx");
  HttpReaderMultirangeTest(batched_retrieval_infos, expected_string);
}

}  // namespace kysync
