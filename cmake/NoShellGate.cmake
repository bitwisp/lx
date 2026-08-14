if(NOT DEFINED LX_SOURCE_DIR)
    get_filename_component(LX_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

file(
    GLOB_RECURSE lx_production_sources
    LIST_DIRECTORIES FALSE
    "${LX_SOURCE_DIR}/include/*.h"
    "${LX_SOURCE_DIR}/include/*.hpp"
    "${LX_SOURCE_DIR}/src/*.c"
    "${LX_SOURCE_DIR}/src/*.cc"
    "${LX_SOURCE_DIR}/src/*.cpp"
    "${LX_SOURCE_DIR}/src/*.cxx"
)

set(lx_forbidden_patterns
    "(^|[^A-Za-z0-9_])system[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])popen[ \t\r\n]*\\("
    "/bin/sh"
    "/bin/bash"
    "(^|[^A-Za-z0-9_])ss([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])lsof([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])systemctl([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])journalctl([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])netstat([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])fuser([^A-Za-z0-9_]|$)"
)

foreach(lx_source IN LISTS lx_production_sources)
    file(READ "${lx_source}" lx_contents)
    foreach(lx_pattern IN LISTS lx_forbidden_patterns)
        if(lx_contents MATCHES "${lx_pattern}")
            message(
                FATAL_ERROR
                "No-shell policy violation in ${lx_source}: ${CMAKE_MATCH_0}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "No-shell policy check passed")

