add_subdirectory(checksums)
add_subdirectory(commands)
add_subdirectory(readers)
add_subdirectory(streams)

add_subdirectory(test_commands)
add_subdirectory(test_http_servers)
add_subdirectory(tests)

add_executable(kysync
        main.cc)
target_link_libraries(kysync
        PRIVATE ky_common
        PRIVATE ky_observability
        PRIVATE kysync_commands
        PRIVATE gflags
        PRIVATE glog::glog)

get_nginx_conf_template_dir(NGINX_CONF_TEMPLATE_DIR)
configure_file(path_config.h.in ${CMAKE_BINARY_DIR}/include/kysync/path_config.h)
