add_executable(lx_benchmarks phase10b_benchmarks.cpp)
target_link_libraries(
    lx_benchmarks
    PRIVATE
        lx::application
        lx::core
        lx::linux
        lx_warnings
        benchmark::benchmark
        benchmark::benchmark_main
)
lx_enable_hardening(lx_benchmarks)
