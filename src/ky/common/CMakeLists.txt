add_library(ky_common INTERFACE
        include/ky/min.h
        include/ky/safe_cast.h)
target_include_directories(ky_common
        INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_executable(safe_cast_tests
        safe_cast_tests.cc)
target_link_libraries(safe_cast_tests
        PRIVATE
        ky_common
        glog::glog
        GTest::gtest
        GTest::gtest_main)
gtest_discover_tests(safe_cast_tests)

