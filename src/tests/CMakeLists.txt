add_subdirectory(http)

add_library(test_main
        utilities/temp_path.cc
        commands/gen_data_command.cc
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
        http)

add_test(NAME performance_tests COMMAND perf_tests)

add_executable(http_tests
        http_tests.cc)
target_link_libraries(http_tests
        test_main
        http)

add_test(NAME http_tests COMMAND http_tests)

add_executable(gendata
        commands/gen_data_command.cc
        gendata_main.cc)
target_link_libraries(gendata
        main)

set_tests_properties(
        tests
        performance_tests
        http_tests

        PROPERTIES
        ENVIRONMENT TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test_data)
