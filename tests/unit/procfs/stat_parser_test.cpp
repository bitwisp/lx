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
