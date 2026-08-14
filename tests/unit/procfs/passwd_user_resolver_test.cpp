#include "lx/linux/procfs/PasswdUserResolver.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <unistd.h>

TEST_CASE("passwd resolver resolves the current user")
{
    const lx::linux::procfs::PasswdUserResolver resolver;
    const auto result = resolver.nameForUid(static_cast<std::uint32_t>(::getuid()));
    REQUIRE(result);
    REQUIRE_FALSE(result.value().empty());
}

TEST_CASE("passwd resolver reports an unknown UID")
{
    const lx::linux::procfs::PasswdUserResolver resolver;
    const auto result = resolver.nameForUid(UINT32_MAX);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::NotFound);
}
