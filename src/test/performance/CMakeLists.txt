add_library(performance_test_common
        performance_test_fixture.cc
        performance_test_profile.cc)
target_link_libraries(performance_test_common
        PUBLIC test_common)

add_executable(performance_tests
        performance_tests.cc)
target_link_libraries(performance_tests
        PRIVATE test_common
        PRIVATE performance_test_common
        PRIVATE ky_observability)
gtest_discover_tests(performance_tests)

add_executable(linux_cache_tests
        linux_cache_tests.cc)
target_link_libraries(linux_cache_tests
        PRIVATE test_common
        PRIVATE performance_test_common)
gtest_discover_tests(linux_cache_tests)
