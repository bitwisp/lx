#include "lx/linux/systemd/SystemdServiceProvider.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("systemd provider rejects invalid PID lookup")
{
    const lx::linux::SystemdServiceProvider provider;
    const auto result = provider.unitByPid(0);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::InvalidArgument);
}
