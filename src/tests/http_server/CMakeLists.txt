add_library(http_server
        http_server.cc
        )
target_link_libraries(http_server
        glog::glog
        httplib::httplib)

add_library(nginx_server
        nginx_server.cc)
target_link_libraries(nginx_server
        PRIVATE glog::glog)

add_executable(nginx_server_tests
        nginx_server_tests.cc)
target_link_libraries(nginx_server_tests
        PRIVATE test_main
        PRIVATE httplib::httplib
        PRIVATE nginx_server
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main)
