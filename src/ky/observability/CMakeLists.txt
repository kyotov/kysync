add_library(ky_observability
        observer.cc
        observable.cc)
target_link_libraries(ky_observability
        PUBLIC ky_timer
        PRIVATE glog::glog)
target_include_directories(ky_observability
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
