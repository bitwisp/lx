if(NOT DEFINED LX_BINARY_DIR OR NOT DEFINED LX_EXPECTED_VERSION)
    message(FATAL_ERROR "Install test requires LX_BINARY_DIR and LX_EXPECTED_VERSION")
endif()

set(lx_install_root "${LX_BINARY_DIR}/install-test-root")
file(REMOVE_RECURSE "${lx_install_root}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${LX_BINARY_DIR}"
            --prefix "${lx_install_root}"
    RESULT_VARIABLE lx_install_result
    OUTPUT_VARIABLE lx_install_output
    ERROR_VARIABLE lx_install_error
)
if(NOT lx_install_result EQUAL 0)
    message(FATAL_ERROR
        "Isolated install failed:\n${lx_install_output}\n${lx_install_error}")
endif()

set(lx_expected_files
    "bin/lx"
    "share/doc/lx/LICENSE"
    "share/doc/lx/README.md"
    "share/doc/lx/development.md"
    "share/doc/lx/json-schema-v1.md"
    "share/doc/lx/security.md"
    "share/man/man1/lx.1"
    "share/bash-completion/completions/lx"
    "share/zsh/site-functions/_lx"
    "share/fish/vendor_completions.d/lx.fish"
)
foreach(lx_relative_path IN LISTS lx_expected_files)
    if(NOT EXISTS "${lx_install_root}/${lx_relative_path}")
        message(FATAL_ERROR "Installed file is missing: ${lx_relative_path}")
    endif()
endforeach()

function(lx_run_installed lx_name)
    execute_process(
        COMMAND "${lx_install_root}/bin/lx" ${ARGN}
        RESULT_VARIABLE lx_result
        OUTPUT_VARIABLE lx_output
        ERROR_VARIABLE lx_error
        TIMEOUT 10
    )
    if(NOT lx_result EQUAL 0)
        message(FATAL_ERROR
            "Installed lx ${lx_name} failed (${lx_result}):\n${lx_output}\n${lx_error}")
    endif()
    set(LX_LAST_OUTPUT "${lx_output}" PARENT_SCOPE)
endfunction()

lx_run_installed("version" --version)
if(NOT LX_LAST_OUTPUT MATCHES "LX ${LX_EXPECTED_VERSION}")
    message(FATAL_ERROR "Unexpected installed version: ${LX_LAST_OUTPUT}")
endif()

lx_run_installed("help" --help)
foreach(lx_command status process port service log inspect find doctor tui)
    if(NOT LX_LAST_OUTPUT MATCHES "${lx_command}")
        message(FATAL_ERROR "Installed help omits command: ${lx_command}")
    endif()
endforeach()

lx_run_installed("doctor" doctor --no-color)
lx_run_installed("status" status --json)
if(NOT LX_LAST_OUTPUT MATCHES "\"schema_version\":1")
    message(FATAL_ERROR "Installed status did not return schema version 1 JSON")
endif()

foreach(lx_completion
        "share/bash-completion/completions/lx"
        "share/zsh/site-functions/_lx"
        "share/fish/vendor_completions.d/lx.fish")
    file(READ "${lx_install_root}/${lx_completion}" lx_completion_text)
    foreach(lx_command status process port service log inspect find doctor tui)
        if(NOT lx_completion_text MATCHES "${lx_command}")
            message(FATAL_ERROR
                "${lx_completion} omits command: ${lx_command}")
        endif()
    endforeach()
endforeach()
