cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE}/install)

find_package(GTest REQUIRED NO_MODULE)
find_package(xxHash REQUIRED NO_MODULE)
find_package(gflags REQUIRED NO_MODULE)
find_package(glog REQUIRED NO_MODULE)

add_library(main STATIC
        readers/Reader.cpp
        readers/MemoryReader.cpp
        readers/FileReader.cpp
        commands/PrepareCommand.cpp
        commands/SyncCommand.cpp
        checksums/wcs.cpp
        checksums/scs.cpp)
target_link_libraries(main
        glog::glog)

add_executable(tests
        tests.cpp
        metrics/ExpectationCheckMetricsVisitor.cpp
        )
target_link_libraries(tests
        main
        xxHash::xxhash
        gflags
        GTest::gtest
        GTest::gtest_main)
