cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE}/install)

find_package(GTest REQUIRED NO_MODULE)

add_library(wcs
        wcs.cpp)

add_executable(tests
        wcs
        tests.cpp)
set_property(TARGET tests PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(tests
        GTest::gtest
        GTest::gtest_main)