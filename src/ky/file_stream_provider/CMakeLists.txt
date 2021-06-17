add_library(ky_file_stream_provider
        file_stream_provider.cc)
target_link_libraries(ky_file_stream_provider
        PRIVATE glog::glog)
target_include_directories(ky_file_stream_provider
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
