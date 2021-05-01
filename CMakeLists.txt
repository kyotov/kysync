cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

include(kydeps/KyDeps.cmake)
KyDeps(DOWNLOAD NO_UPLOAD DEPENDS gflags glog GTest xxHash httplib protobuf zstd)

# TODO: figure out libraries
#       https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=msvc-160

#if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#    add_link_options(-static)
#endif ()

include(CTest)
enable_testing()

set(PROTO_DIR ${CMAKE_BINARY_DIR}/proto)
execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_DIR})
include_directories(${PROTO_DIR})

protobuf_generate(
        PROTOS src/commands/pb/header.proto
        OUT_VAR HEADER_PROTO
        LANGUAGE cpp
        PROTOC_OUT_DIR ${PROTO_DIR})

find_package(ZLIB REQUIRED)

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

add_subdirectory(src/tests)

add_executable(kysync
        src/main.cc)
target_link_libraries(kysync
        main)
