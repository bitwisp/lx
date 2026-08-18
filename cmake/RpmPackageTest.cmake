foreach(lx_required LX_BINARY_DIR LX_CPACK_COMMAND LX_RPM_COMMAND
                    LX_EXPECTED_VERSION)
    if(NOT DEFINED ${lx_required})
        message(FATAL_ERROR "RPM test requires ${lx_required}")
    endif()
endforeach()

set(lx_package_dir "${LX_BINARY_DIR}/packages")
file(REMOVE_RECURSE "${lx_package_dir}")
execute_process(
    COMMAND "${LX_CPACK_COMMAND}"
            --config "${LX_BINARY_DIR}/CPackConfig.cmake" -G RPM
    RESULT_VARIABLE lx_cpack_result
    OUTPUT_VARIABLE lx_cpack_output
    ERROR_VARIABLE lx_cpack_error
    TIMEOUT 180
)
if(NOT lx_cpack_result EQUAL 0)
    message(FATAL_ERROR
        "RPM creation failed:\n${lx_cpack_output}\n${lx_cpack_error}")
endif()

file(GLOB lx_rpms "${lx_package_dir}/*.rpm")
list(LENGTH lx_rpms lx_rpm_count)
if(NOT lx_rpm_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one RPM, found ${lx_rpm_count}")
endif()
list(GET lx_rpms 0 lx_rpm)
file(GLOB lx_checksums "${lx_package_dir}/*.rpm.sha256")
list(LENGTH lx_checksums lx_checksum_count)
if(NOT lx_checksum_count EQUAL 1)
    message(FATAL_ERROR "Expected one RPM checksum, found ${lx_checksum_count}")
endif()
list(GET lx_checksums 0 lx_checksum_file)
file(SHA256 "${lx_rpm}" lx_actual_checksum)
file(READ "${lx_checksum_file}" lx_checksum_contents)
string(FIND "${lx_checksum_contents}" "${lx_actual_checksum}" lx_checksum_position)
if(lx_checksum_position EQUAL -1)
    message(FATAL_ERROR "RPM checksum does not match package")
endif()

execute_process(
    COMMAND "${LX_RPM_COMMAND}" -qp
            --queryformat "%{NAME}\n%{VERSION}\n%{ARCH}\n%{LICENSE}\n" "${lx_rpm}"
    RESULT_VARIABLE lx_info_result
    OUTPUT_VARIABLE lx_info
    ERROR_VARIABLE lx_info_error
)
if(NOT lx_info_result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect RPM: ${lx_info_error}")
endif()
string(REPLACE "\n" ";" lx_info_fields "${lx_info}")
list(GET lx_info_fields 0 lx_name)
list(GET lx_info_fields 1 lx_version)
list(GET lx_info_fields 2 lx_architecture)
list(GET lx_info_fields 3 lx_license)
if(NOT lx_name STREQUAL "lx-resource-manager")
    message(FATAL_ERROR "Unexpected RPM package name: ${lx_name}")
endif()
if(NOT lx_version STREQUAL "${LX_EXPECTED_VERSION}")
    message(FATAL_ERROR "Unexpected RPM version: ${lx_version}")
endif()
if(NOT lx_architecture STREQUAL "x86_64")
    message(FATAL_ERROR "Unexpected RPM architecture: ${lx_architecture}")
endif()
if(NOT lx_license STREQUAL "Apache-2.0")
    message(FATAL_ERROR "Unexpected RPM license: ${lx_license}")
endif()

execute_process(
    COMMAND "${LX_RPM_COMMAND}" -qp --requires "${lx_rpm}"
    RESULT_VARIABLE lx_requires_result
    OUTPUT_VARIABLE lx_requires
    ERROR_VARIABLE lx_requires_error
)
if(NOT lx_requires_result EQUAL 0 OR
   NOT lx_requires MATCHES "libsystemd\\.so\\.0")
    message(FATAL_ERROR "RPM dependency scan failed: ${lx_requires_error}${lx_requires}")
endif()

execute_process(
    COMMAND "${LX_RPM_COMMAND}" -qlp "${lx_rpm}"
    RESULT_VARIABLE lx_files_result
    OUTPUT_VARIABLE lx_files
    ERROR_VARIABLE lx_files_error
)
if(NOT lx_files_result EQUAL 0)
    message(FATAL_ERROR "Unable to list RPM files: ${lx_files_error}")
endif()
foreach(lx_path
        "/usr/bin/lx"
        "/usr/share/man/man1/lx.1"
        "/usr/share/doc/lx/security.md"
        "/usr/share/bash-completion/completions/lx"
        "/usr/share/zsh/site-functions/_lx"
        "/usr/share/fish/vendor_completions.d/lx.fish")
    string(FIND "${lx_files}" "${lx_path}" lx_path_position)
    if(lx_path_position EQUAL -1)
        message(FATAL_ERROR "RPM omits ${lx_path}")
    endif()
endforeach()
