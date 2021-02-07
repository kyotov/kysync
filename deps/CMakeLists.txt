cmake_minimum_required(VERSION 3.17)

project(deps)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

#set(variables
#        CMAKE_C_FLAGS_DEBUG
#        CMAKE_C_FLAGS_MINSIZEREL
#        CMAKE_C_FLAGS_RELEASE
#        CMAKE_C_FLAGS_RELWITHDEBINFO
#        CMAKE_CXX_FLAGS_DEBUG
#        CMAKE_CXX_FLAGS_MINSIZEREL
#        CMAKE_CXX_FLAGS_RELEASE
#        CMAKE_CXX_FLAGS_RELWITHDEBINFO
#        )
#message(STATUS "Initial build flags:")
#foreach(variable ${variables})
#    message(STATUS "  '${variable}': ${${variable}}")
#endforeach()
#message(STATUS "")

include(ExternalProject)

function(install NAME GIT_REPO GIT_TAG)
    ExternalProject_Add(
            ${NAME}
            GIT_REPOSITORY ${GIT_REPO}
            GIT_TAG ${GIT_TAG}
            PREFIX ${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}

            ${ARGN}
            CMAKE_ARGS
            --config ${CMAKE_BUILD_TYPE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/install
    )
endfunction()

install(gflags
        https://github.com/gflags/gflags.git
        v2.2.0

        CMAKE_ARGS
        -DREGISTER_INSTALL_PREFIX=FALSE
        -DGFLAGS_BUILD_TESTING=FALSE)

install(glog
        https://github.com/google/glog.git
        v0.4.0

        DEPENDS gflags

        CMAKE_ARGS
        -DBUILD_TESTING=FALSE)

install(googletest
        https://github.com/google/googletest.git
        release-1.10.0

        CMAKE_ARGS
        #FIXME: figure out how to make all of them static!!!
        -Dgtest_force_shared_crt=ON)

install(xxhash
        https://github.com/Cyan4973/xxHash.git
        v0.8.0

        SOURCE_SUBDIR cmake_unofficial

        CMAKE_ARGS
        -DBUILD_SHARED_LIBS=FALSE
        -DXXHASH_BUILD_XXHSUM=FALSE
        -DXXHASH_BUILD_ENABLE_INLINE_API=FALSE)
