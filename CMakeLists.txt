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
option(LX_ENABLE_SYSTEMD "Enable the systemd sd-bus backend when available" ON)
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

set(LX_HAS_SYSTEMD OFF)
if(LX_ENABLE_SYSTEMD)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSYSTEMD QUIET IMPORTED_TARGET libsystemd)
    endif()

    if(TARGET PkgConfig::LIBSYSTEMD)
        set(LX_SYSTEMD_TARGET PkgConfig::LIBSYSTEMD)
        set(LX_HAS_SYSTEMD ON)
    else()
        find_path(LX_SYSTEMD_INCLUDE_DIR systemd/sd-bus.h)
        find_library(LX_SYSTEMD_LIBRARY NAMES systemd)
        if(LX_SYSTEMD_INCLUDE_DIR AND LX_SYSTEMD_LIBRARY)
            add_library(lx_systemd_dependency UNKNOWN IMPORTED)
            set_target_properties(
                lx_systemd_dependency
                PROPERTIES
                    IMPORTED_LOCATION "${LX_SYSTEMD_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${LX_SYSTEMD_INCLUDE_DIR}"
            )
            set(LX_SYSTEMD_TARGET lx_systemd_dependency)
            set(LX_HAS_SYSTEMD ON)
        endif()
    endif()
endif()

if(LX_HAS_SYSTEMD)
    message(STATUS "systemd sd-bus backend: enabled")
else()
    message(STATUS "systemd sd-bus backend: unavailable")
endif()

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

add_library(lx_linux STATIC
    src/linux/systemd/UnavailableJournalProvider.cpp
    src/linux/systemd/UnavailableServiceProvider.cpp
    src/linux/process/LinuxSignalProvider.cpp
    src/linux/process/PidFd.cpp
    src/linux/netlink/InetDiagCodec.cpp
    src/linux/netlink/NetlinkSocket.cpp
    src/linux/netlink/NetlinkSocketProvider.cpp
    src/linux/procfs/CmdlineParser.cpp
    src/linux/procfs/PasswdUserResolver.cpp
    src/linux/procfs/ProcFsReader.cpp
    src/linux/procfs/SocketFdTarget.cpp
    src/linux/procfs/SocketInodeResolver.cpp
    src/linux/procfs/ProcFsProcessProvider.cpp
    src/linux/procfs/ProcFsSystemMetricsProvider.cpp
    src/linux/procfs/StatParser.cpp
    src/linux/procfs/StatusParser.cpp
    src/linux/procfs/SystemMetricsParser.cpp
)
if(LX_HAS_SYSTEMD)
    target_sources(
        lx_linux
        PRIVATE
            src/linux/systemd/SdBus.cpp
            src/linux/systemd/SdJournal.cpp
            src/linux/systemd/JournalFieldParser.cpp
            src/linux/systemd/SystemdJournalProvider.cpp
            src/linux/systemd/SystemdMessageParser.cpp
            src/linux/systemd/SystemdServiceProvider.cpp
    )
endif()
add_library(lx::linux ALIAS lx_linux)
target_link_libraries(lx_linux PUBLIC lx::core PRIVATE lx_warnings)
if(LX_HAS_SYSTEMD)
    target_link_libraries(lx_linux PUBLIC ${LX_SYSTEMD_TARGET})
    target_compile_definitions(lx_linux PUBLIC LX_HAS_SYSTEMD=1)
else()
    target_compile_definitions(lx_linux PUBLIC LX_HAS_SYSTEMD=0)
endif()
lx_enable_sanitizers(lx_linux)

add_library(
    lx_application STATIC
    src/application/DoctorService.cpp
    src/application/FindService.cpp
    src/application/LogService.cpp
    src/application/MetricsService.cpp
    src/application/InspectService.cpp
    src/application/ProcessService.cpp
    src/application/PortService.cpp
    src/application/ResourceResolver.cpp
    src/application/ServiceService.cpp
    src/application/StatusService.cpp
)
add_library(lx::application ALIAS lx_application)
target_link_libraries(lx_application PUBLIC lx::core PRIVATE lx_warnings)
lx_enable_sanitizers(lx_application)

add_library(lx_cli STATIC src/cli/CliApp.cpp src/cli/JsonSerializer.cpp)
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
    PRIVATE lx_warnings CLI11::CLI11 fmt::fmt nlohmann_json::nlohmann_json
)
lx_enable_sanitizers(lx_cli)

add_executable(lx src/main.cpp)
target_link_libraries(lx PRIVATE lx::cli lx::linux lx_warnings)
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
    add_test(
        NAME no_shell_gate
        COMMAND
            "${CMAKE_COMMAND}"
            -DLX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NoShellGate.cmake"
    )
endif()
