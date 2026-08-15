#include "lx/linux/procfs/StatParser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stat parser accepts names containing spaces and parentheses")
{
    const auto result = lx::linux::procfs::parseStat(
        "9273 (worker (blue) pool) S 1 2 3 4\n");

    REQUIRE(result);
    REQUIRE(result.value().pid == 9273);
    REQUIRE(result.value().name == "worker (blue) pool");
    REQUIRE(result.value().state == 'S');
    REQUIRE(result.value().ppid == 1);
}

TEST_CASE("stat parser rejects malformed records")
{
    REQUIRE_FALSE(lx::linux::procfs::parseStat("9273 worker S 1"));
    REQUIRE_FALSE(lx::linux::procfs::parseStat("bad (worker) S 1"));
    REQUIRE_FALSE(lx::linux::procfs::parseStat("9273 (worker) S bad"));
    REQUIRE_FALSE(lx::linux::procfs::parseStat("9273 (worker)"));
}

TEST_CASE("stat parser reads process CPU counters and start time")
{
    const auto result = lx::linux::procfs::parseProcessCpuStat(
        "42 (demo worker) R 1 2 3 4 5 6 7 8 9 10 100 25 13 14 15 16 17 18 900 20\n");
    REQUIRE(result);
    CHECK(result.value().pid == 42);
    CHECK(result.value().cpuTimeTicks == 125);
    CHECK(result.value().startTimeTicks == 900);
}

TEST_CASE("process CPU stat parser rejects short and overflowing records")
{
    CHECK_FALSE(lx::linux::procfs::parseProcessCpuStat("42 (demo) S 1 2 3\n"));
    CHECK_FALSE(lx::linux::procfs::parseProcessCpuStat(
        "42 (demo) R 1 2 3 4 5 6 7 8 9 10 18446744073709551615 1 13 14 15 16 17 18 20\n"));
}
