add_subdirectory(commands)
add_subdirectory(common)
add_subdirectory(http_servers)
add_subdirectory(performance)

# FIXME: temporarily disabling boost as a dependency for lighter release
#
#add_executable(dependency_tests
#        dependency_tests.cc)
#target_link_libraries(dependency_tests
#        PRIVATE Boost::regex
#        PRIVATE glog::glog
#        PRIVATE GTest::gtest
#        PRIVATE GTest::gtest_main)
#gtest_discover_tests(dependency_tests)

add_executable(basic_tests
        basic_tests.cc)
target_link_libraries(basic_tests
        PRIVATE test_common
        PRIVATE kysync_readers
        PRIVATE kysync_checksums
        PRIVATE httplib::httplib
#        PRIVATE Boost::headers
        PRIVATE zstd::libzstd_static)
gtest_discover_tests(basic_tests)

add_executable(http_tests
        http_tests.cc)
target_link_libraries(http_tests
        PRIVATE test_common
        PRIVATE kysync_readers
        PRIVATE httplib::httplib)
gtest_discover_tests(http_tests)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/log)
