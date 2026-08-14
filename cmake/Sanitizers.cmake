function(lx_enable_sanitizers target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        if(LX_ENABLE_ASAN OR LX_ENABLE_UBSAN)
            message(FATAL_ERROR "Configured sanitizers require GCC or Clang")
        endif()
        return()
    endif()

    set(lx_sanitizers "")
    if(LX_ENABLE_ASAN)
        list(APPEND lx_sanitizers address)
    endif()
    if(LX_ENABLE_UBSAN)
        list(APPEND lx_sanitizers undefined)
    endif()

    if(lx_sanitizers)
        list(JOIN lx_sanitizers "," lx_sanitizer_flags)
        target_compile_options(
            ${target}
            PRIVATE -fsanitize=${lx_sanitizer_flags} -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE -fsanitize=${lx_sanitizer_flags})
    endif()
endfunction()

