#include "lx/application/ProcessService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class FakeProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(const pid_t pid) const override
    {
        lx::ProcessInfo info;
        info.pid = pid;
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(info), {}});
    }
};

class FakeSignalProvider final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(
        const pid_t pid, const lx::ProcessSignal signal) const override
    {
        deliveredPid = pid;
        deliveredSignal = signal;
        return lx::Result<lx::SignalDelivery>::success(
            {pid, signal, lx::SignalMechanism::PidFd});
    }

    lx::Result<bool> waitForExit(
        const pid_t pid, const std::chrono::milliseconds) const override
    {
        waitedPid = pid;
        return lx::Result<bool>::success(true);
    }

    lx::SignalCapabilities capabilities() const override
    {
        return {true, true};
    }

    mutable pid_t deliveredPid = -1;
    mutable lx::ProcessSignal deliveredSignal = lx::ProcessSignal::Terminate;
    mutable pid_t waitedPid = -1;
};

} // namespace

TEST_CASE("ProcessService delegates valid process inspection")
{
    const FakeProcessProvider provider;
    const FakeSignalProvider signals;
    const lx::application::ProcessService service{provider, signals, 99};

    const auto result = service.inspect(42);

    REQUIRE(result);
    REQUIRE(result.value().value.pid == 42);
}

TEST_CASE("ProcessService rejects non-positive PIDs")
{
    const FakeProcessProvider provider;
    const FakeSignalProvider signals;
    const lx::application::ProcessService service{provider, signals, 99};

    const auto result = service.inspect(0);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::InvalidArgument);
}

TEST_CASE("ProcessService delegates protected signal actions")
{
    const FakeProcessProvider provider;
    const FakeSignalProvider signals;
    const lx::application::ProcessService service{provider, signals, 99};

    const auto stopped = service.stop(42);
    REQUIRE(stopped);
    REQUIRE(signals.deliveredPid == 42);
    REQUIRE(signals.deliveredSignal == lx::ProcessSignal::Terminate);

    const auto killed = service.kill(43);
    REQUIRE(killed);
    REQUIRE(signals.deliveredSignal == lx::ProcessSignal::Kill);

    const auto exited = service.waitForExit(42, std::chrono::milliseconds{1});
    REQUIRE(exited);
    REQUIRE(signals.waitedPid == 42);
}

TEST_CASE("ProcessService refuses PID 1 and its own process")
{
    const FakeProcessProvider provider;
    const FakeSignalProvider signals;
    const lx::application::ProcessService service{provider, signals, 99};

    REQUIRE_FALSE(service.stop(1));
    REQUIRE(service.stop(1).error().code == lx::ErrorCode::Conflict);
    REQUIRE_FALSE(service.kill(99));
    REQUIRE(service.kill(99).error().code == lx::ErrorCode::Conflict);
    REQUIRE(signals.deliveredPid == -1);
}
