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

void CheckRangeGet(
    httplib::Client& client,
    const std::string& data,
    int beg,
    int end) {
  auto expected_result = data.substr(beg, end - beg + 1);

  auto range = httplib::make_range_header({{beg, end}});
  auto response = client.Get("/test.data", {range});
  EXPECT_TRUE(response.error() == httplib::Error::Success);
  EXPECT_EQ(response->body, expected_result);
}

TEST(HttpServer, Test1) {  // NOLINT
  auto path = TempPath();
  auto server = HttpServer(path.GetPath());
  auto client = httplib::Client("localhost", 8000);

  std::string data = "0123456789";
  WriteFile(path.GetPath() / "test.data", data);

  auto response = client.Get("/test.data");
  EXPECT_TRUE(response.error() == httplib::Error::Success);
  EXPECT_EQ(response->body, data);

  CheckRangeGet(client, data, 0, 0);
  CheckRangeGet(client, data, 0, 1024);
  CheckRangeGet(client, data, 2, 1024);
  CheckRangeGet(client, data, 2, 4);
  CheckRangeGet(client, data, 0, data.size());

  // TODO: add tests for multiple ranges at the same time
  // TODO: add tests for ranges that end on -1
}

}  // namespace kysync
