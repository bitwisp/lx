#include "lx/contracts/ISignalProvider.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class FakeSignalProvider final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(
        const pid_t pid, const lx::ProcessSignal signal) const override
    {
        return lx::Result<lx::SignalDelivery>::success(
            {pid, signal, lx::SignalMechanism::PidFd});
    }

    lx::Result<bool> waitForExit(
        const pid_t, const std::chrono::milliseconds) const override
    {
        return lx::Result<bool>::success(true);
    }

    lx::SignalCapabilities capabilities() const override
    {
        return {true, true};
    }
};

} // namespace

TEST_CASE("signal provider contract reports delivery and lifecycle")
{
    const FakeSignalProvider provider;
    const auto delivery = provider.send(42, lx::ProcessSignal::Terminate);
    const auto exited = provider.waitForExit(42, std::chrono::milliseconds{10});

    REQUIRE(delivery);
    REQUIRE(delivery.value().mechanism == lx::SignalMechanism::PidFd);
    REQUIRE(exited);
    REQUIRE(exited.value());
    REQUIRE(provider.capabilities().pidFdAvailable);
}
