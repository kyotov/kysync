add_library(ky_process
        process_${CMAKE_SYSTEM_NAME}.cc)
target_link_libraries(ky_process
        PRIVATE glog::glog)
target_include_directories(ky_process
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_library(ky_test_file_chatter
        test_file_chatter.cc)
target_link_libraries(ky_test_file_chatter
        PUBLIC ky_timer
        PRIVATE glog::glog)

add_executable(test_chat_bot
        test_chat_bot.cc)
target_include_directories(test_chat_bot
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include)
target_link_libraries(test_chat_bot
        PRIVATE ky_common
        PRIVATE ky_process
        PRIVATE ky_test_file_chatter
        PRIVATE gflags
        PRIVATE glog::glog)

add_executable(process_tests
        process_tests.cc)
target_include_directories(process_tests
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include)
target_link_libraries(process_tests
        PRIVATE ky_temp_path
        PRIVATE ky_test_file_chatter
        PRIVATE ky_process
        PRIVATE glog::glog
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main)
gtest_discover_tests(process_tests)

file(GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include/kysync/test_locations.h
        INPUT test_locations.h.in)