add_library(kysync_test_commands
        gen_data_command.cc
        system_command.cc)
target_link_libraries(kysync_test_commands
        PUBLIC ky_observability
        PRIVATE ky_common
        PRIVATE ky_parallelize
        PRIVATE kysync_streams
        PRIVATE glog::glog)
target_interface_set_relative_path(kysync_test_commands "kysync/test_commands")

add_executable(gendata
        gendata_main.cc)
target_link_libraries(gendata
        PRIVATE ky_common
        PRIVATE ky_observability
        PRIVATE ky_parallelize
        PRIVATE kysync_streams
        PRIVATE kysync_test_commands
        PRIVATE gflags
        PRIVATE glog::glog)
