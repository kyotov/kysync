function(check_non_empty NAME)
    if ("${${NAME}}" STREQUAL "")
        message(FATAL_ERROR "ERROR: ${NAME} not specified")
    endif ()
endfunction()

check_non_empty(PORT)
check_non_empty(CONF_TEMPLATE_PATH)
check_non_empty(TARGET_DIR)

configure_file("${CONF_TEMPLATE_PATH}" "${TARGET_DIR}/nginx.conf")
