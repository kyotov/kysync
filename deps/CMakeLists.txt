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
            PREFIX ${CMAKE_BINARY_DIR}

            ${ARGN}
            CMAKE_ARGS
            #--config ${CMAKE_BUILD_TYPE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/install
    )
endfunction()

IF (WIN32)
    ExternalProject_Add(
            openssl
            URL https://github.com/CristiFati/Prebuilt-Binaries/raw/master/OpenSSL/v1.1.1/OpenSSL-1.1.1i-Win-pc064.zip
            PREFIX ${CMAKE_BINARY_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ${CMAKE_COMMAND} -E echo installing from `<SOURCE_DIR>/OpenSSL/1.1.1i` to `<INSTALL_DIR>/install`
            COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/OpenSSL/1.1.1i/include <INSTALL_DIR>/install/include
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/OpenSSL/1.1.1i/lib/libssl_static.lib <INSTALL_DIR>/install/lib/ssl.lib
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/OpenSSL/1.1.1i/lib/libcrypto_static.lib <INSTALL_DIR>/install/lib/crypto.lib
            COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/OpenSSL/1.1.1i/bin <INSTALL_DIR>/install/bin
    )
ELSE()
    ExternalProject_Add(
            openssl
            GIT_REPOSITORY https://github.com/openssl/openssl.git
            GIT_TAG OpenSSL_1_1_1-stable
            PREFIX ${CMAKE_BINARY_DIR}
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> ./config no-shared no-dso --prefix=<INSTALL_DIR>/install --openssldir=<INSTALL_DIR>/install
            BUILD_COMMAND make -C <SOURCE_DIR> build_libs #-j
            INSTALL_COMMAND make -C <SOURCE_DIR> install_dev
    )
ENDIF()

install(googletest
        https://github.com/google/googletest.git
        release-1.10.0

        CMAKE_ARGS
        #FIXME: figure out how to make all of them static!!!
        -Dgtest_force_shared_crt=ON)

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

install(xxhash
        https://github.com/Cyan4973/xxHash.git
        v0.8.0

        SOURCE_SUBDIR cmake_unofficial

        CMAKE_ARGS
        -DBUILD_SHARED_LIBS=FALSE
        -DXXHASH_BUILD_XXHSUM=FALSE
        -DXXHASH_BUILD_ENABLE_INLINE_API=FALSE)

install(httplib
        https://github.com/yhirose/cpp-httplib.git
        v0.8.4

        DEPENDS openssl googletest

        CMAKE_ARGS
        -DOPENSSL_USE_STATIC_LIBS=TRUE)

install(zstd
        https://github.com/facebook/zstd.git
        v1.4.7

        SOURCE_SUBDIR build/cmake

        CMAKE_ARGS
        -DZSTD_BUILD_STATIC=TRUE
        -DZSTD_BUILD_SHARED=FALSE)
