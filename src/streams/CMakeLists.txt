add_library(kysync_streams
        streams.cc)
target_link_libraries(kysync_streams
        PRIVATE kysync_checksums
        PRIVATE glog::glog)
target_interface_set_relative_path(kysync_streams "kysync")
