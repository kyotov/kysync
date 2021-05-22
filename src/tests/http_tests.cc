#include <glog/logging.h>
#include <gtest/gtest.h>
#include <httplib.h>
#include <kysync/path_config.h>

#include <array>
#include <fstream>
#include <sstream>

#include "http_server/http_server.h"
#include "utilities/fixture.h"
#include "utilities/temp_path.h"

namespace kysync {

namespace fs = std::filesystem;

class HttpTests : public Fixture {};

void WriteFile(const fs::path& path, const std::string& data) {
  std::ofstream f(path);
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
  input.read(buffer.data(), prefix.size());
  CHECK_EQ(input.gcount(), prefix.size());
  CHECK(std::equal(prefix.begin(), prefix.end(), buffer.begin()));
}

static void ReadLine(std::istream& input, std::string& buffer, int max) {
  for (auto state = 0; state >= 0 && buffer.size() < max;) {
    char c;
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
    }
  }
}

using ChunkCallback =
    std::function<void(size_t /*beg*/, size_t /*end*/, std::istream& /*data*/)>;

static void ParseMultipartByterangesResponse(
    httplib::Response response,
    ChunkCallback chunk_callback) {
  auto content_type = response.get_header_value("Content-Type");
  CHECK(content_type.starts_with("multipart/byteranges"));

  std::string boundary;
  CHECK(httplib::detail::parse_multipart_boundary(content_type, boundary));

  auto dash = std::string("--");
  auto crlf = std::string("\r\n");
  auto buffer = std::string();

  auto body = std::stringstream(response.body);

  size_t beg = 0;
  size_t end = 0;

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
    }
  }
}

template <typename Client>
void CheckGet(const fs::path& path, Client& client) {
  std::string data = "0123456789";
  WriteFile(path / "test.data", data);

  {
    auto response = client.Get("/test.data");
    EXPECT_TRUE(response.error() == httplib::Error::Success);
    EXPECT_EQ(response->body, data);
  }

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
        [&](size_t beg, size_t end, std::istream& s) {
          auto len = end - beg + 1;
          std::vector<char> buffer(len);
          s.read(buffer.data(), len);
          auto chunk = data.substr(beg, len);
          EXPECT_TRUE(std::equal(chunk.begin(), chunk.end(), buffer.begin()));
        });
  }
}

TEST_F(HttpTests, HttpsServer) {  // NOLINT
  auto tmp = TempPath();
  auto port = 8000;
  auto cert_path = fs::path(
      GetEnv("TEST_DATA_DIR", (CMAKE_SOURCE_DIR / "test_data").string()));
  auto server = HttpServer(cert_path, tmp.path, port, true);
  auto client = httplib::SSLClient("localhost", port);
  client.enable_server_certificate_verification(false);

  CheckGet(tmp.path, client);
}

TEST_F(HttpTests, HttpServer) {  // NOLINT
  auto tmp = TempPath();
  auto port = 8000;
  auto server = HttpServer(tmp.path, port, true);
  auto client = httplib::Client("localhost", port);

  CheckGet(tmp.path, client);
}

}  // namespace kysync
