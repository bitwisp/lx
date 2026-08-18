foreach(lx_required LX_PACKAGE_MANAGER LX_OLD_PACKAGE LX_CURRENT_PACKAGE
                    LX_OLD_VERSION LX_CURRENT_VERSION)
    if(NOT DEFINED ${lx_required})
        message(FATAL_ERROR "Package lifecycle test requires ${lx_required}")
    endif()
endforeach()

if(NOT DEFINED ENV{HOME} OR "$ENV{HOME}" STREQUAL "")
    message(FATAL_ERROR "Package lifecycle test requires HOME")
endif()
set(lx_user_config "$ENV{HOME}/.config/lx/config.toml")
file(MAKE_DIRECTORY "$ENV{HOME}/.config/lx")
set(lx_sentinel "# package lifecycle sentinel\nkeep = true\n")
file(WRITE "${lx_user_config}" "${lx_sentinel}")

function(lx_run lx_description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE lx_result
        OUTPUT_VARIABLE lx_output
        ERROR_VARIABLE lx_error
        TIMEOUT 180
    )
    if(NOT lx_result EQUAL 0)
        message(FATAL_ERROR
            "${lx_description} failed (${lx_result}):\n${lx_output}\n${lx_error}")
    endif()
    set(LX_LAST_OUTPUT "${lx_output}" PARENT_SCOPE)
endfunction()

function(lx_install lx_package)
    if(LX_PACKAGE_MANAGER STREQUAL "apt")
        lx_run("apt package installation" apt-get install --yes "${lx_package}")
    elseif(LX_PACKAGE_MANAGER STREQUAL "dnf")
        lx_run("dnf package installation" dnf install --assumeyes "${lx_package}")
    else()
        message(FATAL_ERROR "Unsupported package manager: ${LX_PACKAGE_MANAGER}")
    endif()
endfunction()

function(lx_assert_version lx_expected)
    lx_run("lx version check" /usr/bin/lx --version)
    if(NOT LX_LAST_OUTPUT MATCHES "LX ${lx_expected}")
        message(FATAL_ERROR "Expected LX ${lx_expected}, got: ${LX_LAST_OUTPUT}")
    endif()
    if(LX_PACKAGE_MANAGER STREQUAL "apt")
        lx_run("DEB database version check"
            dpkg-query --show --showformat=\${Version} lx-resource-manager)
    else()
        lx_run("RPM database version check"
            rpm -q --queryformat "%{VERSION}" lx-resource-manager)
    endif()
    if(NOT LX_LAST_OUTPUT STREQUAL "${lx_expected}")
        message(FATAL_ERROR
            "Package database version is ${LX_LAST_OUTPUT}, expected ${lx_expected}")
    endif()
    lx_run("doctor smoke test" /usr/bin/lx doctor --no-color)
    lx_run("status JSON smoke test" /usr/bin/lx status --json)
    if(NOT LX_LAST_OUTPUT MATCHES "\"schema_version\":1")
        message(FATAL_ERROR "Installed status output is not schema v1 JSON")
    endif()
    lx_run("port smoke test" /usr/bin/lx port --no-color)
endfunction()

function(lx_assert_user_config)
    if(NOT EXISTS "${lx_user_config}")
        message(FATAL_ERROR "Package operation deleted user configuration")
    endif()
    file(READ "${lx_user_config}" lx_config_contents)
    if(NOT lx_config_contents STREQUAL "${lx_sentinel}")
        message(FATAL_ERROR "Package operation modified user configuration")
    endif()
endfunction()

lx_install("${LX_OLD_PACKAGE}")
lx_assert_version("${LX_OLD_VERSION}")
lx_assert_user_config()

lx_install("${LX_CURRENT_PACKAGE}")
lx_assert_version("${LX_CURRENT_VERSION}")
lx_assert_user_config()

if(LX_PACKAGE_MANAGER STREQUAL "apt")
    lx_run("DEB package removal"
        apt-get remove --yes lx-resource-manager)
else()
    lx_run("RPM package removal"
        dnf remove --assumeyes lx-resource-manager)
endif()

foreach(lx_owned_path
        /usr/bin/lx
        /usr/share/man/man1/lx.1
        /usr/share/bash-completion/completions/lx
        /usr/share/zsh/site-functions/_lx
        /usr/share/fish/vendor_completions.d/lx.fish)
    if(EXISTS "${lx_owned_path}")
        message(FATAL_ERROR "Package removal left ${lx_owned_path}")
    endif()
endforeach()
lx_assert_user_config()
