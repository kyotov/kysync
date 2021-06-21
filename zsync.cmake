include_guard(GLOBAL)

include("ExternalProject")

if (UNIX AND NOT APPLE AND NOT SKIP_ZSYNC)
    set(ZSYNC_SUPPORTED true)

    ExternalProject_Add(zsync
            URL "http://zsync.moria.org.uk/download/zsync-0.6.2.tar.bz2"
            URL_HASH SHA1=5e69f084c8adaad6a677b68f7388ae0f9507617a
            PREFIX "zsync"

            PATCH_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> patch -u http.c -i ${CMAKE_SOURCE_DIR}/zsync.patch
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> ./configure --prefix=<INSTALL_DIR>
            BUILD_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> make
            INSTALL_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> make install)
else ()
    set(ZSYNC_SUPPORTED false)
endif ()
