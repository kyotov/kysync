add_executable(prepare_command_test
        prepare_command_test.cc)
target_link_libraries(prepare_command_test
        GTest::gtest
        GTest::gtest_main)
add_test(NAME prepare_command_test COMMAND prepare_command_test)
