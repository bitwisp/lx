#include "lx/domain/ProcessSignal.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal delivery identifies its target and mechanism")
{
    const lx::SignalDelivery delivery{
        42, lx::ProcessSignal::Kill, lx::SignalMechanism::PidFd};
    const lx::SignalCapabilities capabilities{true, true};

    REQUIRE(delivery.pid == 42);
    REQUIRE(delivery.signal == lx::ProcessSignal::Kill);
    REQUIRE(delivery.mechanism == lx::SignalMechanism::PidFd);
    REQUIRE(capabilities.signalingAvailable);
    REQUIRE(capabilities.pidFdAvailable);
}
