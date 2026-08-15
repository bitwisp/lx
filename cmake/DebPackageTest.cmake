foreach(lx_required LX_BINARY_DIR LX_CPACK_COMMAND LX_DPKG_DEB_COMMAND
                    LX_EXPECTED_VERSION)
    if(NOT DEFINED ${lx_required})
        message(FATAL_ERROR "DEB test requires ${lx_required}")
    endif()
endforeach()

set(lx_package_dir "${LX_BINARY_DIR}/packages")
file(REMOVE_RECURSE "${lx_package_dir}")
execute_process(
    COMMAND "${LX_CPACK_COMMAND}"
            --config "${LX_BINARY_DIR}/CPackConfig.cmake" -G DEB
    RESULT_VARIABLE lx_cpack_result
    OUTPUT_VARIABLE lx_cpack_output
    ERROR_VARIABLE lx_cpack_error
    TIMEOUT 120
)
if(NOT lx_cpack_result EQUAL 0)
    message(FATAL_ERROR
        "DEB creation failed:\n${lx_cpack_output}\n${lx_cpack_error}")
endif()

file(GLOB lx_debs "${lx_package_dir}/*.deb")
list(LENGTH lx_debs lx_deb_count)
if(NOT lx_deb_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one DEB, found ${lx_deb_count}")
endif()
list(GET lx_debs 0 lx_deb)

function(lx_deb_field lx_field lx_output_name)
    execute_process(
        COMMAND "${LX_DPKG_DEB_COMMAND}" --field "${lx_deb}" "${lx_field}"
        RESULT_VARIABLE lx_field_result
        OUTPUT_VARIABLE lx_field_output
        ERROR_VARIABLE lx_field_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT lx_field_result EQUAL 0)
        message(FATAL_ERROR
            "Unable to read DEB ${lx_field}: ${lx_field_error}")
    endif()
    set(${lx_output_name} "${lx_field_output}" PARENT_SCOPE)
endfunction()

lx_deb_field(Package lx_package)
lx_deb_field(Version lx_version)
lx_deb_field(Architecture lx_architecture)
lx_deb_field(Depends lx_dependencies)
if(NOT lx_package STREQUAL "lx-resource-manager")
    message(FATAL_ERROR "Unexpected DEB package name: ${lx_package}")
endif()
if(NOT "${lx_version}" STREQUAL "${LX_EXPECTED_VERSION}")
    message(FATAL_ERROR "Unexpected DEB version: ${lx_version}")
endif()
if(lx_architecture STREQUAL "")
    message(FATAL_ERROR "DEB architecture is empty")
endif()
foreach(lx_dependency libc6 libstdc++6 libsystemd0)
    string(FIND "${lx_dependencies}" "${lx_dependency}" lx_dependency_position)
    if(lx_dependency_position EQUAL -1)
        message(FATAL_ERROR
            "DEB dependencies omit ${lx_dependency}: ${lx_dependencies}")
    endif()
endforeach()

execute_process(
    COMMAND "${LX_DPKG_DEB_COMMAND}" --contents "${lx_deb}"
    RESULT_VARIABLE lx_contents_result
    OUTPUT_VARIABLE lx_contents
    ERROR_VARIABLE lx_contents_error
)
if(NOT lx_contents_result EQUAL 0)
    message(FATAL_ERROR "Unable to list DEB contents: ${lx_contents_error}")
endif()
foreach(lx_path
        "./usr/bin/lx"
        "./usr/share/man/man1/lx.1"
        "./usr/share/bash-completion/completions/lx"
        "./usr/share/zsh/site-functions/_lx"
        "./usr/share/fish/vendor_completions.d/lx.fish")
    string(FIND "${lx_contents}" "${lx_path}" lx_path_position)
    if(lx_path_position EQUAL -1)
        message(FATAL_ERROR "DEB omits ${lx_path}")
    endif()
endforeach()

set(lx_extract_root "${LX_BINARY_DIR}/deb-extract-root")
file(REMOVE_RECURSE "${lx_extract_root}")
file(MAKE_DIRECTORY "${lx_extract_root}")
execute_process(
    COMMAND "${LX_DPKG_DEB_COMMAND}" --extract "${lx_deb}" "${lx_extract_root}"
    RESULT_VARIABLE lx_extract_result
    ERROR_VARIABLE lx_extract_error
)
if(NOT lx_extract_result EQUAL 0)
    message(FATAL_ERROR "Unable to extract DEB: ${lx_extract_error}")
endif()
execute_process(
    COMMAND "${lx_extract_root}/usr/bin/lx" --version
    RESULT_VARIABLE lx_version_result
    OUTPUT_VARIABLE lx_version_output
    ERROR_VARIABLE lx_version_error
    TIMEOUT 10
)
if(NOT lx_version_result EQUAL 0 OR
   NOT lx_version_output MATCHES "LX ${LX_EXPECTED_VERSION}")
    message(FATAL_ERROR
        "Packaged binary failed version check: ${lx_version_error}${lx_version_output}")
endif()
execute_process(
    COMMAND "${lx_extract_root}/usr/bin/lx" status --json
    RESULT_VARIABLE lx_status_result
    OUTPUT_VARIABLE lx_status_output
    ERROR_VARIABLE lx_status_error
    TIMEOUT 10
)
if(NOT lx_status_result EQUAL 0 OR
   NOT lx_status_output MATCHES "\"schema_version\":1")
    message(FATAL_ERROR
        "Packaged binary failed status check: ${lx_status_error}${lx_status_output}")
endif()
