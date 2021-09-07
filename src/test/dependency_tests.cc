#include <glog/logging.h>
#include <gtest/gtest.h>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/regex.hpp>

namespace kysync {

TEST(Boost, Test1) {  // NOLINT
  std::string line = "Subject: Hello, World!";
  boost::regex pat("^Subject: (Re: |Aw: )*(.*)");
  boost::smatch matches;
  EXPECT_TRUE(boost::regex_match(line, matches, pat));
  EXPECT_EQ(matches[2], "Hello, World!");
}

TEST(Boost, ThreadPool) {  // NOLINT
  int num_threads = 4;
  boost::asio::thread_pool pool(num_threads);
  std::atomic<int> value(0);
  for (int i = 0; i < num_threads * 2; i++) {
    boost::asio::post(pool, [&value]() {
      LOG(INFO) << "Incrementing value";
      value += 1;
    });
  }
  pool.join();
  EXPECT_EQ(value, num_threads * 2);
}

}  // namespace kysync