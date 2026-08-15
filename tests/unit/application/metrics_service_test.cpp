#include "lx/application/MetricsService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class SystemProvider final : public lx::contracts::ISystemMetricsProvider {
public:
    lx::SystemMetricsSample value;
    lx::Result<lx::SystemMetricsSample> sample() const override
    {
        return lx::Result<lx::SystemMetricsSample>::success(value);
    }
};

class ProcessProvider final : public lx::contracts::IProcessMetricsProvider {
public:
    std::vector<lx::ProcessCpuSample> value;
    lx::Result<std::vector<lx::ProcessCpuSample>> sample() const override
    {
        return lx::Result<std::vector<lx::ProcessCpuSample>>::success(value);
    }
};

} // namespace

TEST_CASE("metrics service calculates host and top-style process CPU")
{
    lx::MetricsSnapshot before;
    before.system.cpu = {1000, 600};
    before.system.logicalCpuCount = 4;
    before.processes = {{42, 100, 10}, {43, 200, 20}};

    auto after = before;
    after.system.hostname = "test-host";
    after.system.cpu = {1200, 700};
    after.system.memoryTotalBytes = 1000;
    after.system.memoryAvailableBytes = 250;
    after.processes = {{42, 100, 30}, {43, 201, 100}};

    const auto view = lx::application::MetricsService::compare(before, after);
    REQUIRE(view.host.cpuPercent);
    CHECK(*view.host.cpuPercent == 50.0);
    CHECK(view.host.memoryUsedBytes == 750);
    REQUIRE(view.processCpuPercent.count(42) == 1);
    CHECK(view.processCpuPercent.at(42) == 40.0);
    CHECK(view.processCpuPercent.count(43) == 0); // PID was reused.
}

TEST_CASE("metrics service measure captures on both sides of its waiter")
{
    SystemProvider system;
    ProcessProvider processes;
    system.value.cpu = {100, 50};
    bool waited = false;
    lx::application::MetricsService service(
        system, processes, [&](const auto) {
            waited = true;
            system.value.cpu = {200, 100};
        });

    const auto result = service.measure();
    REQUIRE(result);
    CHECK(waited);
    REQUIRE(result.value().host.cpuPercent);
    CHECK(*result.value().host.cpuPercent == 50.0);
}
