foreach(lx_required LX_EXECUTABLE LX_READELF_COMMAND)
    if(NOT DEFINED ${lx_required})
        message(FATAL_ERROR "ELF hardening test requires ${lx_required}")
    endif()
endforeach()

function(lx_readelf lx_output_name)
    execute_process(
        COMMAND "${LX_READELF_COMMAND}" -W ${ARGN} "${LX_EXECUTABLE}"
        RESULT_VARIABLE lx_result
        OUTPUT_VARIABLE lx_output
        ERROR_VARIABLE lx_error
    )
    if(NOT lx_result EQUAL 0)
        message(FATAL_ERROR "readelf failed: ${lx_error}")
    endif()
    set(${lx_output_name} "${lx_output}" PARENT_SCOPE)
endfunction()

lx_readelf(lx_header -h)
if(NOT lx_header MATCHES "Type:[ ]+DYN")
    message(FATAL_ERROR "Hardened executable is not PIE")
endif()

lx_readelf(lx_program_headers -l)
if(NOT lx_program_headers MATCHES "GNU_RELRO")
    message(FATAL_ERROR "Hardened executable has no GNU_RELRO segment")
endif()
string(REGEX MATCH "GNU_STACK[^\n]*" lx_stack_line "${lx_program_headers}")
if(lx_stack_line STREQUAL "" OR lx_stack_line MATCHES "RWE")
    message(FATAL_ERROR "Hardened executable has an executable stack")
endif()

lx_readelf(lx_dynamic -d)
if(NOT lx_dynamic MATCHES "BIND_NOW")
    message(FATAL_ERROR "Hardened executable does not use immediate binding")
endif()
