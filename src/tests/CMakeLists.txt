add_library(test_common
        test_environment.cc
        fixture.cc
        performance_test_fixture.cc
        performance_test_profile.cc
        expectation_check_metrics_visitor.cc)
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
target_include_directories(test_common
        PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)

add_executable(dependency_tests
        dependency_tests.cc)
target_link_libraries(dependency_tests
        PRIVATE Boost::regex
        PRIVATE glog::glog
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main)
gtest_discover_tests(dependency_tests)

add_executable(basic_tests
        tests.cc)
target_link_libraries(basic_tests
        PRIVATE test_common
        PRIVATE kysync_readers
        PRIVATE kysync_checksums
        PRIVATE httplib::httplib
        PRIVATE zstd::libzstd_static
        PRIVATE Boost::headers)
gtest_discover_tests(basic_tests)

add_executable(performance_tests
        performance_tests.cc)
target_link_libraries(performance_tests
        PRIVATE test_common
        PRIVATE ky_observability)
gtest_discover_tests(performance_tests)

add_executable(http_tests
        http_tests.cc)
target_link_libraries(http_tests
        PRIVATE test_common
        PRIVATE kysync_readers
        PRIVATE httplib::httplib)
gtest_discover_tests(http_tests)

add_executable(linux_cache_tests
        linux_cache_tests.cc)
target_link_libraries(linux_cache_tests
        PRIVATE test_common)
gtest_discover_tests(linux_cache_tests)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/log)
