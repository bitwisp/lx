#include "lx/domain/PortRelease.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("port release result can request a force phase")
{
    lx::PortReleasePlan plan;
    plan.localPort = 8080;
    plan.ownerPids = {10, 20};

    lx::PortReleaseResult result;
    result.deliveries = {
        {10, lx::ProcessSignal::Terminate, lx::SignalMechanism::PidFd},
        {20, lx::ProcessSignal::Terminate, lx::SignalMechanism::Kill},
    };
    result.remaining = plan;

    REQUIRE_FALSE(result.released);
    REQUIRE(result.deliveries.size() == 2);
    REQUIRE(result.remaining->localPort == 8080);
}
