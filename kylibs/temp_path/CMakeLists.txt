add_library(ky_temp_path
        temp_path.cc)
target_link_libraries(ky_temp_path
        glog::glog)
target_interface_set_relative_path(ky_temp_path "ky")
