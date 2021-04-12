cmake_minimum_required(VERSION 3.17)

project(deps)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS FALSE)

include(ExternalProject)

function(install NAME GIT_REPO GIT_TAG)
    ExternalProject_Add(
            ${NAME}
            GIT_REPOSITORY ${GIT_REPO}
            GIT_TAG ${GIT_TAG}
            PREFIX ${CMAKE_BINARY_DIR}

            #            BUILD_COMMAND ${CMAKE_COMMAND} -E echo "Starting $<CONFIG> build"
            #            COMMAND       ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> -j
            #            COMMAND       ${CMAKE_COMMAND} -E echo "$<CONFIG> build complete"

            ${ARGN}
            CMAKE_ARGS
            ##
            #TODO: this suddently started failing on GitHub CI... cmake version!?
            #
            #--config ${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/install
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
            -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
            -DBUILD_SHARED_LIBS=FALSE
    )
endfunction()

IF (WIN32)
    # based on https://stackoverflow.com/a/41815728/4720732

    ##
    # TODO: not sure at all how to inline this...
    #
    file(WRITE ${CMAKE_BINARY_DIR}/zlib.nmake
            AS=ml64\n
            LOC="-DASMV -DASMINF -DNDEBUG -I."\n
            OBJA="inffasx64.obj gvmat64.obj inffas8664.obj"\n
            -f win32/Makefile.msc\n
            zlib.lib\n)
    ExternalProject_Add(
            zlib
            GIT_REPOSITORY https://github.com/kyotov/zlib.git
            GIT_TAG mt
            PREFIX ${CMAKE_BINARY_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> nmake @${CMAKE_BINARY_DIR}/zlib.nmake
            INSTALL_COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/zlib.h <INSTALL_DIR>/install/include/zlib.h
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/zconf.h <INSTALL_DIR>/install/include/zconf.h
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/zlib.lib <INSTALL_DIR>/install/lib/zlib.lib
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/zlib.pdb <INSTALL_DIR>/install/lib/zlib.pdb
    )

    ExternalProject_Add(
            perl
            URL https://strawberryperl.com/download/5.32.1.1/strawberry-perl-5.32.1.1-64bit-portable.zip
            PREFIX ${CMAKE_BINARY_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
    )

    ExternalProject_Add(
            openssl
            GIT_REPOSITORY https://github.com/openssl/openssl.git
            GIT_TAG OpenSSL_1_1_1-stable
            PREFIX ${CMAKE_BINARY_DIR}
            DEPENDS perl zlib

            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> <INSTALL_DIR>/src/perl/perl/bin/perl Configure VC-WIN64A-masm zlib no-shared no-zlib-dynamic threads --prefix=<INSTALL_DIR>/install --openssldir=<INSTALL_DIR>/install /MT
            COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> <INSTALL_DIR>/src/perl/perl/bin/perl -pi.orig -e s/ZLIB1/zlib.lib/g; makefile

            # TODO: figure out if we can't put some include directory...
            COMMAND ${CMAKE_COMMAND} -E copy <INSTALL_DIR>/install/include/zlib.h <SOURCE_DIR>/zlib.h
            COMMAND ${CMAKE_COMMAND} -E copy <INSTALL_DIR>/install/include/zconf.h <SOURCE_DIR>/zconf.h
            # COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/zlib.lib <INSTALL_DIR>/install/lib/zlib.lib
            # COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/zlib.pdb <INSTALL_DIR>/install/lib/zlib.pdb

            BUILD_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> nmake build_libs

            INSTALL_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> nmake install_dev
    )
ELSE ()
    ExternalProject_Add(
            openssl
            GIT_REPOSITORY https://github.com/openssl/openssl.git
            GIT_TAG OpenSSL_1_1_1-stable
            PREFIX ${CMAKE_BINARY_DIR}
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> ./config no-shared no-dso --prefix=<INSTALL_DIR>/install --openssldir=<INSTALL_DIR>/install
            BUILD_COMMAND make -C <SOURCE_DIR> build_libs
            INSTALL_COMMAND make -C <SOURCE_DIR> install_dev
    )
ENDIF ()

install(googletest
        https://github.com/google/googletest.git
        release-1.10.0

        #CMAKE_ARGS
        #FIXME: figure out how to make all of them static!!!
        #-Dgtest_force_shared_crt=ON
        )

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

install(protobuf
        https://github.com/protocolbuffers/protobuf.git
        v3.15.8

        SOURCE_SUBDIR cmake

        CMAKE_ARGS
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_MSVC_STATIC_RUNTIME=ON)

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
