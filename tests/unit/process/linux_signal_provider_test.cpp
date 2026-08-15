#include "lx/linux/process/LinuxSignalProvider.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>

TEST_CASE("LinuxSignalProvider exposes a deterministic kill fallback")
{
    const lx::linux::process::LinuxSignalProvider provider{
        lx::linux::process::PidFdPolicy::Disabled};

    REQUIRE(provider.capabilities().signalingAvailable);
    REQUIRE_FALSE(provider.capabilities().pidFdAvailable);

    const auto missing = std::numeric_limits<pid_t>::max();
    const auto exited = provider.waitForExit(
        missing, std::chrono::milliseconds{0});
    REQUIRE(exited);
    REQUIRE(exited.value());

    const auto delivery = provider.send(missing, lx::ProcessSignal::Terminate);
    REQUIRE_FALSE(delivery);
    REQUIRE(delivery.error().code == lx::ErrorCode::NotFound);
}
