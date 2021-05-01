add_library(http
        server.cc)
target_link_libraries(http
        glog::glog
        httplib::httplib)

add_executable(cli
        cli.cc)
target_link_libraries(cli
        http)
