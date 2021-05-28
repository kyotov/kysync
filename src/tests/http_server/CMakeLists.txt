add_library(http_server
        http_server.cc
        )
target_link_libraries(http_server
        glog::glog
        httplib::httplib)
