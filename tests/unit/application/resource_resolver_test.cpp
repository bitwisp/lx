#include "lx/application/ResourceResolver.h"
#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <catch2/catch_test_macros.hpp>

namespace {
class Processes final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(pid_t pid) const override
    {
        if (pid != 42 && pid != 8080) {
            return lx::Result<lx::Observation<lx::ProcessInfo>>::failure(
                {lx::ErrorCode::NotFound, "missing", 0, "fake", "get"});
        }
        lx::ProcessInfo process;
        process.pid = pid;
        process.name = "nginx";
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(process), {}});
    }
    lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>> list() const override
    {
        lx::ProcessInfo first;
        first.pid = 42;
        first.name = "nginx";
        lx::ProcessInfo second = first;
        second.pid = 43;
        lx::ProcessInfo third;
        third.pid = 44;
        third.name = "worker";
        lx::ProcessInfo fourth = third;
        fourth.pid = 45;
        return lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>>::success(
            {{first, second, third, fourth}, {}});
    }
};
class Signals final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(pid_t, lx::ProcessSignal) const override
    { return lx::Result<lx::SignalDelivery>::failure({}); }
    lx::Result<bool> waitForExit(pid_t, std::chrono::milliseconds) const override
    { return lx::Result<bool>::success(false); }
    lx::SignalCapabilities capabilities() const override { return {}; }
};
class Sockets final : public lx::contracts::ISocketProvider {
public:
    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(
        const lx::SocketQuery& query) const override
    {
        std::vector<lx::SocketInfo> sockets;
        if (query.localPort && *query.localPort == 8080) {
            lx::SocketInfo socket;
            socket.local.port = 8080;
            socket.inode = 1;
            sockets.push_back(std::move(socket));
        }
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success(
            {std::move(sockets), {}});
    }
};
class Owners final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>&) const override
    {
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success({{}, {}});
    }
};
class Services final : public lx::contracts::IServiceProvider {
public:
    lx::Result<void> probe() const override { return lx::Result<void>::success(); }
    lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>> list() const override
    { return lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>>::success({{}, {}}); }
    lx::Result<lx::Observation<lx::ServiceInfo>> get(
        const std::string& unit) const override
    {
        if (unit != "nginx.service") {
            return lx::Result<lx::Observation<lx::ServiceInfo>>::failure(
                {lx::ErrorCode::NotFound, "missing", 0, "fake", "get"});
        }
        lx::ServiceInfo service;
        service.unitName = unit;
        return lx::Result<lx::Observation<lx::ServiceInfo>>::success(
            {std::move(service), {}});
    }
    lx::Result<std::optional<std::string>> unitByPid(pid_t) const override
    { return lx::Result<std::optional<std::string>>::success(std::nullopt); }
    lx::Result<void> start(const std::string&) const override { return lx::Result<void>::success(); }
    lx::Result<void> stop(const std::string&) const override { return lx::Result<void>::success(); }
    lx::Result<void> restart(const std::string&) const override { return lx::Result<void>::success(); }
};

struct Fixture {
    Processes processes;
    Signals signals;
    Services services;
    lx::application::ServiceService serviceService{services};
    lx::application::ProcessService processService{
        processes, signals, 999, &serviceService};
    Sockets sockets;
    Owners owners;
    lx::application::PortService portService{
        sockets, owners, processService, &serviceService};
    lx::application::ResourceResolver resolver{
        portService, processService, serviceService};
};
} // namespace

TEST_CASE("resource resolver parses explicit resource targets")
{
    Fixture fixture;
    CHECK(std::get<lx::PortTarget>(fixture.resolver.resolve("port:8080").value()).port == 8080);
    CHECK(std::get<lx::ProcessTarget>(fixture.resolver.resolve("pid:42").value()).pid == 42);
    CHECK(std::get<lx::ServiceTarget>(fixture.resolver.resolve("service:nginx").value()).unit == "nginx.service");
    CHECK(fixture.resolver.resolve("file:x").error().code == lx::ErrorCode::InvalidArgument);
}

TEST_CASE("resource resolver reports numeric port and PID ambiguity")
{
    Fixture fixture;
    const auto result = fixture.resolver.resolve("8080");
    REQUIRE_FALSE(result);
    CHECK(result.error().code == lx::ErrorCode::Conflict);
    CHECK(result.error().message.find("port:8080") != std::string::npos);
    CHECK(result.error().message.find("pid:8080") != std::string::npos);
}

TEST_CASE("resource resolver prefers an exact service name")
{
    Fixture fixture;
    const auto result = fixture.resolver.resolve("nginx");
    REQUIRE(result);
    CHECK(std::holds_alternative<lx::ServiceTarget>(result.value()));
}

TEST_CASE("resource resolver reports multiple exact process names")
{
    Fixture fixture;
    const auto result = fixture.resolver.resolve("worker");
    REQUIRE_FALSE(result);
    CHECK(result.error().code == lx::ErrorCode::Conflict);
    CHECK(result.error().message.find("pid:44") != std::string::npos);
}
