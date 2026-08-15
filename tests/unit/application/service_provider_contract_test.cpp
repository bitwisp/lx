#include "lx/linux/systemd/UnavailableServiceProvider.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("unavailable service provider fails without affecting construction")
{
    const lx::linux::UnavailableServiceProvider provider{"backend missing"};

    const auto probe = provider.probe();
    const auto list = provider.list();
    const auto lookup = provider.unitByPid(42);
    const auto restart = provider.restart("demo.service");

    REQUIRE_FALSE(probe);
    REQUIRE(probe.error().code == lx::ErrorCode::Unavailable);
    REQUIRE(probe.error().message == "backend missing");
    REQUIRE_FALSE(list);
    REQUIRE_FALSE(lookup);
    REQUIRE_FALSE(restart);
}
