cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

include(kydeps/KyDeps.cmake)
KyDeps(DOWNLOAD NO_UPLOAD DEPENDS gflags glog GTest xxHash httplib protobuf zstd)

# TODO: figure out libraries
#       https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=msvc-160

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #    add_link_options(-static)
    add_compile_options(--coverage)
    add_link_options(--coverage)
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
        OUT_VAR HEADER_PROTO
        LANGUAGE cpp
        PROTOC_OUT_DIR ${PROTO_DIR})

add_library(main STATIC
        ${HEADER_PROTO}
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
        ${Protobuf_LIBRARIES}
        protobuf::libprotobuf-lite
        protobuf::libprotobuf
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
add_test(NAME tests COMMAND tests)
add_test(NAME performance_tests COMMAND perf_tests)
add_test(NAME http_tests COMMAND http_tests)

set(TEST_ENVIRONMENT
        TEST_ROOT_DIR=${CMAKE_BINARY_DIR}/tmp
        TEST_LOG_DIR=${CMAKE_BINARY_DIR}/log
        TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test_data
        ZSYNC_BIN_DIR=${CMAKE_BINARY_DIR}/zsync/bin)

set_tests_properties(
        tests
        performance_tests
        http_tests

        PROPERTIES
        ENVIRONMENT "${TEST_ENVIRONMENT}")
