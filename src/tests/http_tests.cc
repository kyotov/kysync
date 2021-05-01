#include <gtest/gtest.h>
#include <httplib.h>

#include <fstream>

#include "http/http_server.h"
#include "utilities/temp_path.h"

namespace kysync {

namespace fs = std::filesystem;

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

//  {
//    auto range = httplib::make_range_header({{1, 3}, {5, 7}, {9, -1}});
//    auto response = client.Get("/test.data", {range});
//
//    EXPECT_TRUE(response.error() == httplib::Error::Success);
//    EXPECT_EQ(response->body, "123");
//  }
}

TEST(HttpsServer, Test1) {  // NOLINT
  auto path = TempPath();
  auto port = 8000;
  auto cert_path = fs::path(std::getenv("TEST_DATA_DIR"));
  auto server = HttpServer(cert_path, path.GetPath(), port);
  auto client = httplib::SSLClient("localhost", port);
  client.enable_server_certificate_verification(false);

  CheckGet(path.GetPath(), client);
}

TEST(HttpServer, Test1) {  // NOLINT
  auto path = TempPath();
  auto port = 8000;
  auto server = HttpServer(path.GetPath(), port);
  auto client = httplib::Client("localhost", port);

  CheckGet(path.GetPath(), client);
}

}  // namespace kysync
