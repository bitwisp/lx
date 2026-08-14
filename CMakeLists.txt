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
include(Dependencies)

set(
    LX_DISPLAY_VERSION
    "${PROJECT_VERSION}-dev"
    CACHE STRING
    "LX display version"
)
lx_add_runtime_dependencies()

add_library(lx_core INTERFACE)
add_library(lx::core ALIAS lx_core)
target_include_directories(
    lx_core
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_compile_features(lx_core INTERFACE cxx_std_17)

configure_file(
    cmake/Version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/lx/Version.h"
    @ONLY
)

add_library(lx_warnings INTERFACE)
lx_set_project_warnings(lx_warnings)

add_library(lx_application STATIC src/application/DoctorService.cpp)
add_library(lx::application ALIAS lx_application)
target_link_libraries(lx_application PUBLIC lx::core PRIVATE lx_warnings)
lx_enable_sanitizers(lx_application)

add_library(lx_cli STATIC src/cli/CliApp.cpp)
add_library(lx::cli ALIAS lx_cli)
target_include_directories(
    lx_cli
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(
    lx_cli
    PUBLIC lx::application lx::core
    PRIVATE lx_warnings CLI11::CLI11 fmt::fmt
)
lx_enable_sanitizers(lx_cli)

add_executable(lx src/main.cpp)
target_link_libraries(lx PRIVATE lx::cli lx_warnings)
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
    lx_add_test_dependencies()
    add_subdirectory(tests)
endif()
