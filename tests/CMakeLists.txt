add_executable(lx_unit_tests unit/smoke_test.cpp)
target_link_libraries(
    lx_unit_tests
    PRIVATE
        lx::core
        lx_warnings
        Catch2::Catch2WithMain
)
lx_enable_sanitizers(lx_unit_tests)

include(Catch)
catch_discover_tests(lx_unit_tests)

