#include "lx/application/StatusService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class SystemProvider final : public lx::contracts::ISystemMetricsProvider {
public:
    mutable std::uint64_t total = 100;
    lx::Result<lx::SystemMetricsSample> sample() const override
    {
        lx::SystemMetricsSample value;
        value.hostname = "host";
        value.cpu = {total, total / 2};
        value.memoryTotalBytes = 1000;
        value.memoryAvailableBytes = 400;
        return lx::Result<lx::SystemMetricsSample>::success(value);
    }
};

class ProcessProvider final : public lx::contracts::IProcessMetricsProvider {
public:
    lx::Result<std::vector<lx::ProcessCpuSample>> sample() const override
    {
        return lx::Result<std::vector<lx::ProcessCpuSample>>::success({});
    }
};

} // namespace

TEST_CASE("status service returns current host status")
{
    SystemProvider system;
    ProcessProvider processes;
    lx::application::MetricsService metrics(
        system, processes, [&](const auto) { system.total = 200; });
    lx::application::StatusService status(metrics);

    const auto result = status.get();
    REQUIRE(result);
    CHECK(result.value().hostname == "host");
    CHECK(result.value().memoryUsedBytes == 600);
    REQUIRE(result.value().cpuPercent);
    CHECK(*result.value().cpuPercent == 50.0);
}
