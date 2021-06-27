add_library(kysync_checksums
        weak_checksum.cc
        strong_checksum.cc
        strong_checksum_builder.cc)
target_link_libraries(kysync_checksums
        PRIVATE xxHash::xxhash)
target_interface_set_relative_path(kysync_checksums "kysync/checksums")
