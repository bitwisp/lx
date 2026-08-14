add_executable(
    lx_unit_tests
    unit/application/doctor_service_test.cpp
    unit/application/process_service_test.cpp
    unit/cli/cli_app_test.cpp
    unit/domain/observation_test.cpp
    unit/domain/result_test.cpp
    unit/procfs/procfs_reader_test.cpp
    unit/procfs/stat_parser_test.cpp
    unit/procfs/status_parser_test.cpp
    unit/smoke_test.cpp
)
target_link_libraries(
    lx_unit_tests
    PRIVATE
        lx::core
        lx::cli
        lx::linux
        lx_warnings
        Catch2::Catch2WithMain
)
lx_enable_sanitizers(lx_unit_tests)

include(Catch)
if(LX_ENABLE_ASAN)
    set(lx_asan_test_environment "ASAN_OPTIONS=detect_leaks=0")
    set_property(
        TARGET lx_unit_tests
        PROPERTY CROSSCOMPILING_EMULATOR
            "${CMAKE_COMMAND};-E;env;${lx_asan_test_environment}"
    )
    catch_discover_tests(
        lx_unit_tests
        PROPERTIES ENVIRONMENT "${lx_asan_test_environment}"
    )
else()
    catch_discover_tests(lx_unit_tests)
endif()
