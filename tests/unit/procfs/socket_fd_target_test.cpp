#include "lx/linux/procfs/SocketFdTarget.h"

#include <catch2/catch_test_macros.hpp>

using lx::linux::procfs::parseSocketFdTarget;

TEST_CASE("socket fd target parser extracts an inode")
{
    const auto result = parseSocketFdTarget("socket:[123456]");

    REQUIRE(result);
    REQUIRE(result.value() == std::optional<std::uint64_t>{123456});
}

TEST_CASE("socket fd target parser ignores other descriptor types")
{
    const auto result = parseSocketFdTarget("pipe:[123456]");

    REQUIRE(result);
    REQUIRE_FALSE(result.value());
}

TEST_CASE("socket fd target parser rejects malformed and overflowing inodes")
{
    REQUIRE_FALSE(parseSocketFdTarget("socket:[]"));
    REQUIRE_FALSE(parseSocketFdTarget("socket:[12x]"));
    REQUIRE_FALSE(parseSocketFdTarget("socket:[12]suffix"));
    REQUIRE_FALSE(parseSocketFdTarget("socket:[18446744073709551616]"));
}
