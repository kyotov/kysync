cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE}/install)

link_directories(${CMAKE_PREFIX_PATH}/lib)

message(${CMAKE_PREFIX_PATH})

find_package(GTest REQUIRED NO_MODULE)
find_package(xxHash REQUIRED NO_MODULE)
find_package(gflags REQUIRED NO_MODULE)
find_package(glog REQUIRED NO_MODULE)
find_package(httplib REQUIRED NO_MODULE PATHS ${CMAKE_PREFIX_PATH}/CMake/httplib)

add_library(main STATIC
        readers/Reader.cpp
        readers/MemoryReader.cpp
        readers/FileReader.cpp
        readers/HttpReader.cpp
        commands/Command.cpp
        commands/PrepareCommand.cpp
        commands/SyncCommand.cpp
        checksums/wcs.cpp
        checksums/StrongChecksum.cpp
        checksums/StrongChecksumBuilder.cpp
        Monitor.cpp)
target_link_libraries(main
        ssl
        crypto
        glog::glog)
#FIXME: this is not portable and also not clear it works well for debug...
#set_target_properties(main PROPERTIES COMPILE_FLAGS /GL)
#set_target_properties(main PROPERTIES LINK_FLAGS /LTCG)

add_executable(kysync
        main.cpp)
target_link_libraries(kysync
        main
        httplib::httplib
        glog::glog
        gflags
        xxHash::xxhash)
#set_target_properties(kysync PROPERTIES COMPILE_FLAGS /GL)
#set_target_properties(kysync PROPERTIES LINK_FLAGS /LTCG)

add_executable(tests
        tests.cpp
        metrics/ExpectationCheckMetricsVisitor.cpp)
target_link_libraries(tests
        main
        xxHash::xxhash
        GTest::gtest
        GTest::gtest_main)
