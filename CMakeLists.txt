cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

include(CTest)
include(GoogleTest)
enable_testing()

include(kydeps/install/KyDeps.cmake)

include(gflags)
include(glog)
include(protobuf)
include(GTest)
include(zstd)
include(httplib)
include(xxHash)
include(llvm)
include(nginx)

set(USE_CLANG_TIDY OFF)

if (USE_CLANG_TIDY)
    string(REGEX REPLACE "[\\\\|/]" "[\\\\/]" HEADERS "${CMAKE_BINARY_DIR}/../.*")
    set(CMAKE_CXX_CLANG_TIDY
            "${CLANG_TIDY}"
            "--header-filter=${HEADERS}"
            "--export-fixes=${CMAKE_BINARY_DIR}/clang-tidy.fixes")
endif ()

include(zsync.cmake)

add_subdirectory(src)