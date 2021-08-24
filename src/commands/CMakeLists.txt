add_subdirectory(pb)

add_library(kysync_commands
        command.cc
        kysync_command.cc
        prepare_command.cc
        sync_command.cc)
target_link_libraries(kysync_commands
        PUBLIC ky_metrics
        PUBLIC ky_observability
        PUBLIC kysync_checksums
        PUBLIC kysync_readers
        PRIVATE ky_common
        PRIVATE ky_parallelize
        PRIVATE ky_file_stream_provider
        PRIVATE kysync_proto
        PRIVATE kysync_streams
        PRIVATE glog::glog
        PRIVATE zstd::libzstd_static)
target_interface_set_relative_path(kysync_commands "kysync/commands")

add_executable(prepare_command_test
        prepare_command_test.cc)
target_link_libraries(prepare_command_test
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main)
add_test(NAME prepare_command_test COMMAND prepare_command_test)
