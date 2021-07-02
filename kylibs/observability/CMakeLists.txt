add_library(ky_observability
        observer.cc
        observable.cc)
target_link_libraries(ky_observability
        PUBLIC ky_timer
        PRIVATE glog::glog)
target_interface_set_relative_path(ky_observability "ky/observability")
