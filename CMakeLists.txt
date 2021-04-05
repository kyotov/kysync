cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_BUILD_TYPE STREQUAL "")
    message("INFO: CMAKE_BUILD_TYPE not specified, assuming Debug")
    set(CMAKE_BUILD_TYPE Debug)
endif ()

# TODO: figure out how to make sure we have enabled IPO/WPO/LTO
#       e.g. on Windows:
#       set_target_properties(kysync PROPERTIES COMPILE_FLAGS /GL)
#       set_target_properties(kysync PROPERTIES LINK_FLAGS /LTCG)

#
# see https://cliutils.gitlab.io/modern-cmake/chapters/basics/variables.html
#
option(FORCE_REFRESH_DEPS "force refresh reps" ON)

set(DEPS_DIR ${CMAKE_BINARY_DIR}/deps)
message("INFO: dependencies will be installed in ${DEPS_DIR}")

if ((NOT EXISTS ${DEPS_DIR}) OR (${FORCE_REFRESH_DEPS}))
    message("${DEPS_DIR} not found, installing deps...")

    execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_DIR}
    )
    execute_process(
            COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -B ${DEPS_DIR} -S ${CMAKE_SOURCE_DIR}/deps -G "${CMAKE_GENERATOR}"
    )
    execute_process(
            ##
            #FIXME: this started failing on mac first (wihtout removing CC and CXX from the environment)
            #       it then started failing on linux too (at least on github ci)
            #       symptoms:
            #       * rerun fixed it...
            #       * but if you remove build/CMakeCache.txt then it is not fixed...
            #       dumped the environment before the call and noticed difference in CC/CXX. made it the same. passed.
            COMMAND ${CMAKE_COMMAND} -E env --unset=CC --unset=CXX ${CMAKE_COMMAND} --build ${DEPS_DIR} --config ${CMAKE_BUILD_TYPE} -j
    )
else ()
    message("${DEPS_DIR} found, skipping deps install...")
endif ()

set(CMAKE_PREFIX_PATH ${DEPS_DIR}/install)

include(CTest)
enable_testing()

find_package(GTest REQUIRED NO_MODULE)
find_package(xxHash REQUIRED NO_MODULE)
find_package(gflags REQUIRED NO_MODULE)
find_package(glog REQUIRED NO_MODULE)
find_package(httplib REQUIRED NO_MODULE PATHS ${CMAKE_PREFIX_PATH}/CMake/httplib)
find_package(zstd REQUIRED NO_MODULE)

add_library(main STATIC
        src/utilities/parallelize.cc
        src/readers/Reader.cc
        src/readers/MemoryReader.cc
        src/readers/FileReader.cc
        src/readers/HttpReader.cc
        src/commands/Command.cc
        src/commands/PrepareCommand.cc
        src/commands/SyncCommand.cc
        src/checksums/wcs.cc
        src/checksums/StrongChecksum.cc
        src/checksums/StrongChecksumBuilder.cc
        src/Monitor.cc)
target_link_libraries(main
        zstd::libzstd_static
        xxHash::xxhash
        httplib::httplib
        glog::glog)

add_executable(kysync
        src/main.cc)
target_link_libraries(kysync
        main
        gflags)

add_executable(tests
        src/tests/tests.cc
        src/metrics/ExpectationCheckMetricsVisitor.cc)
target_link_libraries(tests
        main
        GTest::gtest
        GTest::gtest_main)

add_executable(gendata
        src/tests/gendata.cc)
target_link_libraries(gendata
        main
        gflags
        glog::glog)

add_test(
        NAME tests
        COMMAND tests
)

set_tests_properties(
        tests
        PROPERTIES
        ENVIRONMENT TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test_data
)