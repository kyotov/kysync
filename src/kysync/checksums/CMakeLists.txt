add_library(kysync_checksums
        weak_checksum.cc
        strong_checksum.cc
        strong_checksum_builder.cc)
target_link_libraries(kysync_checksums
        PRIVATE xxHash::xxhash)
target_include_directories(kysync_checksums
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)