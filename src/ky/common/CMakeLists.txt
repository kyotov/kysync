add_library(ky_common
        noexcept.cc)
target_link_libraries(ky_common
        PRIVATE glog::glog)
target_include_directories(ky_common
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_executable(safe_cast_tests
        safe_cast_tests.cc)
target_link_libraries(safe_cast_tests
        PRIVATE
        ky_common
        glog::glog
        GTest::gtest
        GTest::gtest_main)
gtest_discover_tests(safe_cast_tests)

