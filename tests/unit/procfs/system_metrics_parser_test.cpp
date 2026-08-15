#include "lx/linux/procfs/SystemMetricsParser.h"

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("system metrics parser reads aggregate CPU counters")
{
    const auto result = lx::linux::procfs::parseCpuTimes(
        "cpu  10 2 3 40 5 6 7 8 9 10\ncpu0 1 2 3 4\n");
    REQUIRE(result);
    CHECK(result.value().totalTicks == 100);
    CHECK(result.value().idleTicks == 45);
}

TEST_CASE("system metrics parser reads required memory fields")
{
    const auto result = lx::linux::procfs::parseMemoryInfo(
        "MemTotal: 1024 kB\nMemFree: 1 kB\nMemAvailable: 256 kB\n");
    REQUIRE(result);
    CHECK(result.value().totalBytes == 1024U * 1024U);
    CHECK(result.value().availableBytes == 256U * 1024U);
}

TEST_CASE("system metrics parser reads fractional uptime")
{
    const auto result = lx::linux::procfs::parseUptime("123.45 10.00\n");
    REQUIRE(result);
    CHECK(result.value() == 123450ms);
}

TEST_CASE("system metrics parser rejects incomplete records")
{
    CHECK_FALSE(lx::linux::procfs::parseCpuTimes("cpu 1 2 3\n"));
    CHECK_FALSE(lx::linux::procfs::parseMemoryInfo("MemTotal: 1 kB\n"));
    CHECK_FALSE(lx::linux::procfs::parseUptime("not-a-number\n"));
}

TEST_CASE("system metrics parser counts per CPU rows")
{
    CHECK(lx::linux::procfs::countLogicalCpus(
              "cpu 1 2 3 4\ncpu0 1 2 3 4\ncpu1 1 2 3 4\n") == 2);
}
