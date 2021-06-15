cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

include(kydeps/install/KyDeps.cmake)

include(gflags)
include(glog)
include(protobuf)
include(GTest)
include(zstd)
include(httplib)
include(xxHash)
include(llvm)

if (USE_CLANG_TIDY)
    string(REGEX REPLACE "[\\\\|/]" "[\\\\/]" HEADERS "${CMAKE_BINARY_DIR}/../.*")
    set(CMAKE_CXX_CLANG_TIDY
            "${CLANG_TIDY}"
            "--header-filter=${HEADERS}"
            "--export-fixes=${CMAKE_BINARY_DIR}/clang-tidy.fixes")
endif ()

#if (WIN32)
##    add_compile_options(-EHsc)
#    set(HEADERS ${CMAKE_BINARY_DIR}/../src/.*)
#    string(REPLACE "\\" "." HEADERS "${HEADERS}")
#    string(REPLACE "/" "." HEADERS "${HEADERS}")
#    set(CMAKE_CXX_CLANG_TIDY
#            "C:/kamen/clion/kydeps_new/cmake-build-release/install/llvm_b81466e899edbfd2171ec43fd7ea6f0674a824c8/bin/clang-tidy.exe"
#            "--header-filter=${HEADERS}"
#            "--export-fixes=${CMAKE_BINARY_DIR}/clang-tidy.fixes"
#            # --list-checks;
#            # --dump-config;
#            )
#endif ()

# TODO: figure out libraries
#       https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=msvc-160

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #    add_link_options(-static)
    #    add_compile_options(--coverage)
    #    add_link_options(--coverage)
endif ()

include(zsync.cmake)

configure_file(path_config.h.in include/kysync/path_config.h)
include_directories(${CMAKE_BINARY_DIR}/include)

set(PROTO_DIR ${CMAKE_BINARY_DIR}/proto)
execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_DIR})
include_directories(${PROTO_DIR})

protobuf_generate(
        PROTOS src/commands/pb/header.proto
        OUT_VAR PROTO
        LANGUAGE cpp
        PROTOC_OUT_DIR ${PROTO_DIR})

add_library(proto
        src/commands/pb/header_adapter.cc
        ${PROTO})
target_link_libraries(proto
        glog::glog
        ${Protobuf_LIBRARIES}
        protobuf::libprotobuf-lite
        protobuf::libprotobuf)
set_target_properties(proto PROPERTIES CXX_CLANG_TIDY "")

add_library(main STATIC
        src/tests/commands/gen_data_command.cc
        src/utilities/parallelize.cc
        src/utilities/streams.cc
        src/utilities/timer.cc
        src/readers/reader.cc
        src/readers/memory_reader.cc
        src/readers/file_reader.cc
        src/readers/http_reader.cc
        src/commands/command.cc
        src/commands/prepare_command.cc
        src/commands/sync_command.cc
        src/checksums/weak_checksum.cc
        src/checksums/strong_checksum.cc
        src/checksums/strong_checksum_builder.cc
        src/monitor.cc)
target_link_libraries(main
        proto
        zstd::libzstd_static
        xxHash::xxhash
        httplib::httplib
        glog::glog
        gflags)

add_executable(kysync
        src/main.cc)
target_link_libraries(kysync
        main)

include(CTest)
enable_testing()

add_subdirectory(src/tests)
add_subdirectory(src/commands)
add_test(NAME tests COMMAND tests)
add_test(NAME performance_tests COMMAND perf_tests)
add_test(NAME http_tests COMMAND http_tests)
add_test(NAME safe_cast_tests COMMAND safe_cast_tests)