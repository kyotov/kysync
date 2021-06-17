add_library(ky_temp_path
        temp_path.cc)
target_link_libraries(ky_temp_path
        glog::glog)
target_include_directories(ky_temp_path
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
