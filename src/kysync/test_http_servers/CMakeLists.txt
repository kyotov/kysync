function(get_nginx_conf_template_dir OUTPUT_VARIABLE)
    set(${OUTPUT_VARIABLE} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/conf" PARENT_SCOPE)
endfunction()

add_library(kysync_http_server
        http_server.cc)
target_link_libraries(kysync_http_server
        PUBLIC ky_metrics
        PRIVATE glog::glog
        PRIVATE httplib::httplib)
target_include_directories(kysync_http_server
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_library(kysync_nginx_server
        nginx_server.cc)
target_link_libraries(kysync_nginx_server
        PUBLIC ky_metrics
        PRIVATE ky_common
        PRIVATE glog::glog)
target_include_directories(kysync_nginx_server
        PRIVATE "${CMAKE_BINARY_DIR}/include"
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_executable(nginx_server_tests
        nginx_server_tests.cc)
target_link_libraries(nginx_server_tests
        PRIVATE test_common
        PRIVATE httplib::httplib
        PRIVATE kysync_nginx_server
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main)
target_include_directories(nginx_server_tests
        PRIVATE "${CMAKE_BINARY_DIR}/include")
gtest_discover_tests(nginx_server_tests)
