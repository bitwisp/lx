#include "lx/linux/procfs/StatusParser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("status parser extracts identity threads and RSS")
{
    const auto result = lx::linux::procfs::parseStatus(
        "Name:\tdemo\nUid:\t1000\t1001\nUnknown:\tignored\n"
        "Gid:\t100\t101\nVmRSS:\t412 kB\nThreads:\t23\n");
    REQUIRE(result);
    REQUIRE(result.value().uid == 1000);
    REQUIRE(result.value().gid == 100);
    REQUIRE(result.value().threads == 23);
    REQUIRE(result.value().rssBytes == 412 * 1024);
}

TEST_CASE("status parser permits a missing VmRSS field")
{
    const auto result = lx::linux::procfs::parseStatus(
        "Uid:\t0\nGid:\t0\nThreads:\t1\n");
    REQUIRE(result);
    REQUIRE(result.value().rssBytes == 0);
}

TEST_CASE("status parser rejects missing malformed and overflowing fields")
{
    REQUIRE_FALSE(lx::linux::procfs::parseStatus("Uid:\t1\nGid:\t1\n"));
    REQUIRE_FALSE(lx::linux::procfs::parseStatus(
        "Uid:\t-1\nGid:\t1\nThreads:\t1\n"));
    REQUIRE_FALSE(lx::linux::procfs::parseStatus(
        "Uid:\t1\nGid:\t1\nThreads:\tbad\n"));
    REQUIRE_FALSE(lx::linux::procfs::parseStatus(
        "Uid:\t1\nGid:\t1\nThreads:\t1\n"
        "VmRSS:\t18446744073709551615 kB\n"));
}
