file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/kysync)

protobuf_generate(
        PROTOS header.proto
        OUT_VAR PROTO
        LANGUAGE cpp
        PROTOC_OUT_DIR ${CMAKE_BINARY_DIR}/include/kysync)

add_library(kysync_proto
        header_adapter.cc
        ${PROTO})
target_link_libraries(kysync_proto
        PRIVATE glog::glog
        PRIVATE ${Protobuf_LIBRARIES}
        PRIVATE protobuf::libprotobuf-lite
        PRIVATE protobuf::libprotobuf)
target_include_directories(kysync_proto
        PRIVATE ${CMAKE_BINARY_DIR}/include)
set_target_properties(kysync_proto PROPERTIES CXX_CLANG_TIDY "")

