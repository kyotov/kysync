add_library(ky_timer
        timer.cc)
target_include_directories(ky_timer
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
