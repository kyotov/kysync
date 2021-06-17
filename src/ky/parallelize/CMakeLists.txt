add_library(ky_parallelize
        parallelize.cc)
target_link_libraries(ky_parallelize
        PRIVATE glog::glog)
target_include_directories(ky_parallelize
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
