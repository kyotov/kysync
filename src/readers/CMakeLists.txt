add_library(kysync_readers
        reader.cc
        memory_reader.cc
        file_reader.cc
        http_reader.cc
        posix_sequential_file_reader.cc)
target_link_libraries(kysync_readers
        PUBLIC ky_metrics
        PRIVATE ky_common
        PRIVATE glog::glog
        PRIVATE httplib::httplib)
target_interface_set_relative_path(kysync_readers "kysync/readers")
