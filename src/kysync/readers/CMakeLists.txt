add_library(kysync_readers
        reader.cc
        memory_reader.cc
        file_reader.cc
        http_reader.cc)
target_link_libraries(kysync_readers
        PUBLIC ky_metrics
        PRIVATE ky_common
        PRIVATE glog::glog
        PRIVATE httplib::httplib)
target_include_directories(kysync_readers
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)