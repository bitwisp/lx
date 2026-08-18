include(FetchContent)

function(lx_add_runtime_dependencies)
    set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG 37bb6edc5317e99af72ef48405e65d9ca5218861
        GIT_SHALLOW TRUE
    )

    set(FMT_DOC OFF CACHE BOOL "" FORCE)
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
        GIT_SHALLOW TRUE
    )

    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG 55f93686c01528224f448c19128836e7df245f72
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(CLI11 fmt nlohmann_json)
endfunction()

function(lx_add_tui_dependencies)
    set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        FTXUI
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG c100eab535db2283b78d30fcb6d082a1f84fb683
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(FTXUI)
endfunction()

function(lx_add_test_dependencies)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG 6ee0826dcae55ed1e06b2c5701981221e979e1e6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()

function(lx_add_benchmark_dependencies)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG eddb0241389718a23a42db6af5f0164b6e0139af
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(benchmark)
endfunction()
