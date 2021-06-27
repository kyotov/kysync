add_library(ky_common
        noexcept.cc)
target_link_libraries(ky_common
        PRIVATE glog::glog)
target_interface_set_relative_path(ky_common "ky")

add_executable(safe_cast_tests
        safe_cast_tests.cc)
target_link_libraries(safe_cast_tests
        PRIVATE
        ky_common
        glog::glog
        GTest::gtest
        GTest::gtest_main)
gtest_discover_tests(safe_cast_tests)

