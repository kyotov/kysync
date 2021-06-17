add_library(kysync_streams
        streams.cc)
target_link_libraries(kysync_streams
        PRIVATE kysync_checksums
        PRIVATE glog::glog)
target_include_directories(kysync_streams
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
