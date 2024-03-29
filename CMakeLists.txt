cmake_minimum_required(VERSION 3.17)
project(ksync
        VERSION 1)

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
include(nginx)
#include(Boost)

option(USE_CLANG_TIDY "run clang-tidy on the project" OFF)

if (USE_CLANG_TIDY)
    include(llvm)

    string(REGEX REPLACE "[\\\\|/]" "[\\\\/]" HEADERS "${CMAKE_BINARY_DIR}/../.*")
    set(CMAKE_CXX_CLANG_TIDY
            "${CLANG_TIDY}"
            "--header-filter=${HEADERS}"
            "--export-fixes=${CMAKE_BINARY_DIR}/clang-tidy.fixes")
endif ()

option(SKIP_ZSYNC "don't try to build zsync and run zsync perf test even on supported platforms" OFF)

include(zsync.cmake)

function(target_interface_set_relative_path TARGET RELATIVE_PATH)
    set(INTERFACE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include/${RELATIVE_PATH}")

    file(MAKE_DIRECTORY "${INTERFACE_DIR}")
    file(REMOVE_RECURSE "${INTERFACE_DIR}")
    file(CREATE_LINK "${CMAKE_CURRENT_LIST_DIR}/include" "${INTERFACE_DIR}" SYMBOLIC)

    target_include_directories("${TARGET}"
            PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
endfunction()

add_subdirectory(kylibs)
add_subdirectory(src)