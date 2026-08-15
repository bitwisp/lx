#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"

#include <catch2/catch_test_macros.hpp>

#include <unordered_map>
#include <utility>

namespace {

class SocketProvider final : public lx::contracts::ISocketProvider {
public:
    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(
        const lx::SocketQuery&) const override
    {
        lx::SocketInfo first;
        first.inode = 42;
        lx::SocketInfo second;
        second.inode = 84;
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success(
            {{first, second},
             {{lx::ErrorCode::IoError, "socket warning", 0, "fake-socket",
               "query"}}});
    }
};

class OwnerResolver final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>& inodes) const override
    {
        requested = inodes;
        ++resolveCalls;
        if (changeAfterFirst && resolveCalls > 1) {
            return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
                {{{42, {30}}, {84, {10}}}, {}});
        }
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {{{42, {10, 20}}, {84, {10}}},
             {{lx::ErrorCode::PermissionDenied, "owner warning", 0,
               "fake-owner", "resolve"}}});
    }

    mutable std::vector<std::uint64_t> requested;
    bool changeAfterFirst = false;
    mutable int resolveCalls = 0;
};

class ProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(
        const pid_t pid) const override
    {
        ++calls[pid];
        if (pid == 20) {
            return lx::Result<lx::Observation<lx::ProcessInfo>>::failure({
                lx::ErrorCode::NotFound, "process disappeared", 0,
                "fake-process", "get"});
        }
        lx::ProcessInfo info;
        info.pid = pid;
        info.name = "demo";
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(info), {}});
    }

    mutable std::unordered_map<pid_t, int> calls;
};

class SignalProvider final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(
        const pid_t pid, const lx::ProcessSignal signal) const override
    {
        if (pid == failPid) {
            return lx::Result<lx::SignalDelivery>::failure({
                lx::ErrorCode::PermissionDenied, "signal denied", 0,
                "fake-signal", "send"});
        }
        sent.push_back({pid, signal, lx::SignalMechanism::PidFd});
        return lx::Result<lx::SignalDelivery>::success(sent.back());
    }

    lx::Result<bool> waitForExit(
        const pid_t pid, const std::chrono::milliseconds) const override
    {
        waited.push_back(pid);
        return lx::Result<bool>::success(false);
    }

    lx::SignalCapabilities capabilities() const override { return {true, true}; }

    mutable std::vector<lx::SignalDelivery> sent;
    mutable std::vector<pid_t> waited;
    pid_t failPid = -1;
};

class EmptyOwnerResolver final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>&) const override
    {
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {{}, {}});
    }
};

} // namespace

TEST_CASE("PortService enriches sockets with shared process owners")
{
    const SocketProvider sockets;
    const OwnerResolver owners;
    const ProcessProvider processes;
    const SignalProvider signals;
    const lx::application::ProcessService processService{
        processes, signals, 99};
    const lx::application::PortService service{
        sockets, owners, processService, std::chrono::milliseconds{0}};

    const auto result = service.inspect({});

    REQUIRE(result);
    REQUIRE(owners.requested == std::vector<std::uint64_t>{42, 84});
    REQUIRE(result.value().value[0].socket.ownerPids ==
            std::vector<pid_t>{10, 20});
    REQUIRE(result.value().value[0].owners.size() == 1);
    REQUIRE(result.value().value[0].owners.front().name == "demo");
    REQUIRE(result.value().value[1].owners.size() == 1);
    REQUIRE(processes.calls.at(10) == 1);
    REQUIRE(processes.calls.at(20) == 1);
    REQUIRE(result.value().warnings.size() == 3);
}

TEST_CASE("PortService plans and gracefully signals every unique owner")
{
    const SocketProvider sockets;
    const OwnerResolver owners;
    const ProcessProvider processes;
    const SignalProvider signals;
    const lx::application::ProcessService processService{
        processes, signals, 99};
    const lx::application::PortService service{
        sockets, owners, processService, std::chrono::milliseconds{0}};

    const auto plan = service.prepareRelease(8080);
    REQUIRE(plan);
    REQUIRE(plan.value().value.ownerPids == std::vector<pid_t>{10, 20});

    const auto result = service.terminate(plan.value().value);
    REQUIRE(result);
    REQUIRE_FALSE(result.value().value.released);
    REQUIRE(result.value().value.remaining);
    REQUIRE(signals.sent.size() == 2);
    REQUIRE(signals.sent[0].signal == lx::ProcessSignal::Terminate);
    REQUIRE(signals.waited == std::vector<pid_t>{10, 20});
}

TEST_CASE("PortService refuses ownership changes before signaling")
{
    const SocketProvider sockets;
    OwnerResolver owners;
    owners.changeAfterFirst = true;
    const ProcessProvider processes;
    const SignalProvider signals;
    const lx::application::ProcessService processService{
        processes, signals, 99};
    const lx::application::PortService service{
        sockets, owners, processService, std::chrono::milliseconds{0}};

    const auto plan = service.prepareRelease(8080);
    REQUIRE(plan);
    const auto result = service.terminate(plan.value().value);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::Conflict);
    REQUIRE(signals.sent.empty());
}

TEST_CASE("PortService refuses unresolved socket owners")
{
    const SocketProvider sockets;
    const EmptyOwnerResolver owners;
    const ProcessProvider processes;
    const SignalProvider signals;
    const lx::application::ProcessService processService{
        processes, signals, 99};
    const lx::application::PortService service{
        sockets, owners, processService, std::chrono::milliseconds{0}};

    const auto result = service.prepareRelease(8080);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::PermissionDenied);
    REQUIRE(signals.sent.empty());
}

TEST_CASE("PortService reports partial delivery failures")
{
    const SocketProvider sockets;
    const OwnerResolver owners;
    const ProcessProvider processes;
    SignalProvider signals;
    signals.failPid = 20;
    const lx::application::ProcessService processService{
        processes, signals, 99};
    const lx::application::PortService service{
        sockets, owners, processService, std::chrono::milliseconds{0}};
    const auto plan = service.prepareRelease(8080);
    REQUIRE(plan);

    const auto result = service.terminate(plan.value().value);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::PermissionDenied);
    REQUIRE(result.error().message.find("partially") != std::string::npos);
    REQUIRE(signals.sent.size() == 1);
}

TEST_CASE("PortService force phase sends SIGKILL and reports timeout")
{
    const SocketProvider sockets;
    const OwnerResolver owners;
    const ProcessProvider processes;
    const SignalProvider signals;
    const lx::application::ProcessService processService{
        processes, signals, 99};
    const lx::application::PortService service{
        sockets, owners, processService, std::chrono::milliseconds{0}};
    const auto plan = service.prepareRelease(8080);
    REQUIRE(plan);

    const auto result = service.force(plan.value().value);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::Timeout);
    REQUIRE(signals.sent.size() == 2);
    REQUIRE(signals.sent.front().signal == lx::ProcessSignal::Kill);
}
