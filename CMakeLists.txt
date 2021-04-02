cmake_minimum_required(VERSION 3.17)
project(ksync)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_BUILD_TYPE STREQUAL "")
	message("INFO: CMAKE_BUILD_TYPE not specified, assuming Debug")
	set(CMAKE_BUILD_TYPE Debug)
endif ()

#
# see https://cliutils.gitlab.io/modern-cmake/chapters/basics/variables.html
#
option(FORCE_REFRESH_DEPS "force refresh reps" OFF)

set(DEPS_DIR ${CMAKE_BINARY_DIR}/deps/${CMAKE_BUILD_TYPE})
message("INFO: dependencies will be installed in ${DEPS_DIR}")

#if ((NOT EXISTS ${DEPS_DIR}) OR (${FORCE_REFRESH_DEPS}))
if (NOT EXISTS ${DEPS_DIR})
    message("${DEPS_DIR} not found, installing deps...")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_DIR}
    )
    execute_process(
        WORKING_DIRECTORY ${DEPS_DIR}
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${CMAKE_SOURCE_DIR}/deps #-G "${CMAKE_GENERATOR}"
    )
    execute_process(
        WORKING_DIRECTORY ${DEPS_DIR}
        COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    )
else ()
    message("${DEPS_DIR} found, skipping deps install...")
endif () 

set(CMAKE_PREFIX_PATH ${DEPS_DIR}/install)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(CTest)
enable_testing()

#If (APPLE)
#    file(RELATIVE_PATH LD_PATH "${CMAKE_BINARY_DIR}" "${DEPS_DIR}/install/lib")
#    add_link_options(-L${LD_PATH})
#endif ()

find_package(GTest REQUIRED NO_MODULE)
find_package(xxHash REQUIRED NO_MODULE)
find_package(gflags REQUIRED NO_MODULE)
find_package(glog REQUIRED NO_MODULE)
find_package(httplib REQUIRED NO_MODULE PATHS ${CMAKE_PREFIX_PATH}/CMake/httplib)
find_package(zstd REQUIRED NO_MODULE)

# TODO: see if there is no zstd::... to use instead
#if (MSVC)
#   set(ZSTD_LIBRARY zstd_static)
#else ()
#    set(ZSTD_LIBRARY zstd)
#endif ()

add_library(main STATIC
        readers/Reader.cpp
        readers/MemoryReader.cpp
        readers/FileReader.cpp
        readers/HttpReader.cpp
        commands/Command.cpp
        commands/PrepareCommand.cpp
        commands/SyncCommand.cpp
        checksums/wcs.cpp
        checksums/StrongChecksum.cpp
        checksums/StrongChecksumBuilder.cpp
        Monitor.cpp)
target_link_libraries(main
        zstd::libzstd_static
        xxHash::xxhash
        httplib::httplib
        glog::glog)

#FIXME: this is not portable and also not clear it works well for debug...
#set_target_properties(main PROPERTIES COMPILE_FLAGS /GL)
#set_target_properties(main PROPERTIES LINK_FLAGS /LTCG)

add_executable(kysync
        main.cpp)
target_link_libraries(kysync
	main
        gflags)
target_link_options(kysync PRIVATE -undefined dynamic_lookup)

#set_target_properties(kysync PROPERTIES COMPILE_FLAGS /GL)
#set_target_properties(kysync PROPERTIES LINK_FLAGS /LTCG)

add_executable(tests
        tests.cpp
        metrics/ExpectationCheckMetricsVisitor.cpp)
target_link_libraries(tests
        main
        GTest::gtest
        GTest::gtest_main)
target_link_options(tests PRIVATE -undefined dynamic_lookup)

add_test(
        NAME tests
        COMMAND tests
)