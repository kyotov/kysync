include(CTest)
enable_testing()

add_subdirectory(http_server)

add_library(test_main
        commands/gen_data_command.cc
        commands/system_command.cc
        utilities/temp_path.cc
        utilities/fixture.cc
        expectation_check_metrics_visitor.cc)
target_link_libraries(test_main
        main
        GTest::gtest
        GTest::gtest_main)

add_executable(tests
        tests.cc)
target_link_libraries(tests
        test_main)

add_test(NAME tests COMMAND tests)

add_executable(perf_tests
        perf_tests.cc)
target_link_libraries(perf_tests
        test_main
        http_server)

add_test(NAME performance_tests COMMAND perf_tests)

add_executable(http_tests
        http_tests.cc)
target_link_libraries(http_tests
        test_main
        http_server)

add_test(NAME http_tests COMMAND http_tests)

add_executable(gendata
        commands/gen_data_command.cc
        gendata_main.cc)
target_link_libraries(gendata
        main)

add_custom_target(create_log_dir ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/log)

set(TEST_ENVIRONMENT
        TEST_ROOT_DIR=${CMAKE_BINARY_DIR}/tmp
        TEST_LOG_DIR=${CMAKE_BINARY_DIR}/log
        TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test_data
        ZSYNC_BIN_DIR=${CMAKE_BINARY_DIR}/zsync/bin)

set_tests_properties(
        tests
        performance_tests
        http_tests

        PROPERTIES
        ENVIRONMENT "${TEST_ENVIRONMENT}")
