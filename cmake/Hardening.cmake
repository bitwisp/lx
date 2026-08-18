include(CheckCXXCompilerFlag)

function(lx_enable_hardening target)
    if(NOT LX_ENABLE_HARDENING)
        return()
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "LX hardening currently requires GCC or Clang")
    endif()

    foreach(lx_flag -fstack-protector-strong -fstack-clash-protection)
        string(MAKE_C_IDENTIFIER "${lx_flag}" lx_flag_id)
        check_cxx_compiler_flag("${lx_flag}" "LX_HAS_${lx_flag_id}")
        if(LX_HAS_${lx_flag_id})
            target_compile_options(${target} PRIVATE "${lx_flag}")
        endif()
    endforeach()
    target_compile_options(
        ${target} PRIVATE $<$<CONFIG:Release>:-U_FORTIFY_SOURCE>
    )
    target_compile_definitions(
        ${target} PRIVATE $<$<CONFIG:Release>:_FORTIFY_SOURCE=2>
    )
    set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE TRUE)

    get_target_property(lx_target_type ${target} TYPE)
    if(lx_target_type STREQUAL "EXECUTABLE")
        include(CheckPIESupported)
        check_pie_supported(LANGUAGES CXX OUTPUT_VARIABLE lx_pie_error)
        if(NOT CMAKE_CXX_LINK_PIE_SUPPORTED)
            message(FATAL_ERROR "PIE is required for hardened LX builds: ${lx_pie_error}")
        endif()
        target_link_options(
            ${target}
            PRIVATE
                -Wl,-z,relro
                -Wl,-z,now
                -Wl,-z,noexecstack
                -Wl,-z,defs
        )
    endif()
endfunction()
