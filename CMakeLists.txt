cmake_minimum_required(VERSION 3.20)

project(
    LX
    VERSION 0.1.0
    DESCRIPTION "Resource-oriented Linux inspection and management tool"
    LANGUAGES CXX
)

option(LX_BUILD_TESTS "Build LX tests" ON)
option(LX_BUILD_TUI "Build the terminal user interface" OFF)
option(LX_BUILD_BENCHMARKS "Build LX benchmarks" OFF)
option(LX_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LX_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(LX_ENABLE_LTO "Enable link-time optimization" OFF)
option(LX_WARNINGS_AS_ERRORS "Treat LX source warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

add_library(lx_core INTERFACE)
add_library(lx::core ALIAS lx_core)
target_include_directories(
    lx_core
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_compile_features(lx_core INTERFACE cxx_std_17)

add_library(lx_warnings INTERFACE)
lx_set_project_warnings(lx_warnings)

add_executable(lx src/main.cpp)
target_link_libraries(lx PRIVATE lx::core lx_warnings)
lx_enable_sanitizers(lx)

if(LX_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lx_ipo_supported OUTPUT lx_ipo_error)
    if(NOT lx_ipo_supported)
        message(FATAL_ERROR "IPO/LTO is not supported: ${lx_ipo_error}")
    endif()
    set_property(TARGET lx PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(LX_BUILD_TESTS)
    include(CTest)
    enable_testing()
endif()

