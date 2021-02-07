cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE}/install)

find_package(GTest REQUIRED NO_MODULE)
find_package(xxHash REQUIRED NO_MODULE)
find_package(gflags REQUIRED NO_MODULE)
find_package(glog REQUIRED NO_MODULE)

add_library(checksums STATIC
        checksums/wcs.cpp
        checksums/scs.cpp)
target_link_libraries(checksums
        xxHash::xxhash)

add_executable(tests
        tests.cpp
        readers/Reader.cpp
        readers/MemoryReader.cpp
        readers/FileReader.cpp
        commands/PrepareCommand.cpp
        metrics/ExpectationCheckMetricsVisitor.cpp
        )
target_link_libraries(tests
        checksums
        xxHash::xxhash
        gflags
        glog::glog
        GTest::gtest
        GTest::gtest_main)
