#include <glog/logging.h>
#include <gtest/gtest.h>

#include <boost/regex.hpp>

namespace kysync {

TEST(Boost, Test1) {  // NOLINT
  std::string line = "Subject: Hello, World!";
  boost::regex pat("^Subject: (Re: |Aw: )*(.*)");
  boost::smatch matches;
  EXPECT_TRUE(boost::regex_match(line, matches, pat));
  EXPECT_EQ(matches[2], "Hello, World!");
}

}  // namespace kysync