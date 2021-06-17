#include <gtest/gtest.h>
#include <httplib.h>
#include <ky/temp_path.h>
#include <kysync/path_config.h>
#include <kysync/test_http_servers/nginx_server.h>

namespace kysync {

TEST(NginxServer, Simple) {  // NOLINT
  auto tmp = ky::TempPath(CMAKE_BINARY_DIR / "tmp", true);
  auto port = 8000;
  auto server = NginxServer(tmp.GetPath(), port);

  auto client = httplib::Client("localhost", port);

  {
    std::filesystem::current_path(tmp.GetPath());
    std::ofstream f("data.txt");
    f << "Hello, World!" << std::endl;
  }

  auto result = client.Get("/data.txt");
  ASSERT_TRUE(result.error() == httplib::Error::Success);
  ASSERT_EQ(result->status, 200);
  ASSERT_TRUE(result->body.starts_with("Hello, World!"));
}

}  // namespace kysync
