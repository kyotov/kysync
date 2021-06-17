add_library(ky_metrics
        metric_callback_visitor.cc)
target_include_directories(ky_metrics
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
