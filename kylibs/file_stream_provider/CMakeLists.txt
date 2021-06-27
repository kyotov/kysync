add_library(ky_file_stream_provider
        file_stream_provider.cc)
target_link_libraries(ky_file_stream_provider
        PRIVATE glog::glog)
target_interface_set_relative_path(ky_file_stream_provider "ky")
