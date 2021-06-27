add_library(ky_parallelize
        parallelize.cc)
target_link_libraries(ky_parallelize
        PRIVATE glog::glog)
target_interface_set_relative_path(ky_parallelize "ky")
