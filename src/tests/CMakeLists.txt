add_subdirectory(http_server)

add_library(test_main
        commands/gen_data_command.cc
        commands/system_command.cc
        utilities/temp_path.cc
        utilities/fixture.cc
        expectation_check_metrics_visitor.cc)
target_link_libraries(test_main
        main
        http_server
        GTest::gtest
        GTest::gtest_main)

add_executable(tests
        tests.cc)
target_link_libraries(tests
        test_main)

add_executable(perf_tests
        perf_tests.cc)
target_link_libraries(perf_tests
        test_main)

add_executable(http_tests
        http_tests.cc)
target_link_libraries(http_tests
        test_main)

add_executable(gendata
        commands/gen_data_command.cc
        gendata_main.cc)
target_link_libraries(gendata
        main)

add_custom_target(create_log_dir ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/log)
