add_library(test_common
        test_fixture.cc
        expectation_check_metrics_visitor.cc
        test_environment.cc)
target_link_libraries(test_common
        PUBLIC
        ky_common
        ky_metrics
        ky_temp_path
        kysync_test_commands
        kysync_commands
        kysync_nginx_server
        kysync_http_server
        glog::glog
        GTest::gtest
        GTest::gtest_main)
target_interface_set_relative_path(test_common "kysync/test_common")
target_include_directories(test_common
        PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)
