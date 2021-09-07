set(KYDEPS_QUICK OFF
        CACHE BOOL "download from s3" FORCE)

set(KYDEPS_PACKAGES
        google/test
        google/flags
        google/log
        google/protobuf
        zstd
        xxHash
        httplib
        nginx
        CACHE STRING "packages to build" FORCE)
