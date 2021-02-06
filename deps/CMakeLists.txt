cmake_minimum_required(VERSION 3.17)

project(deps)

include(ExternalProject)

ExternalProject_Add(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.10.0

        PREFIX ${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}
        CMAKE_ARGS
        --config ${CMAKE_BUILD_TYPE}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/install
)
