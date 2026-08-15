#include "lx/application/InspectService.h"
#include "lx/application/LogService.h"
#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ResourceResolver.h"
#include "lx/application/ServiceService.h"

#include <catch2/catch_test_macros.hpp>

namespace {
class Processes final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(pid_t pid) const override
    {
        if (pid != 10) return lx::Result<lx::Observation<lx::ProcessInfo>>::failure(
            {lx::ErrorCode::NotFound, "missing process", 0, "fake", "get"});
        lx::ProcessInfo process;
        process.pid = pid;
        process.name = "demo";
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success({process, {}});
    }
    lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>> list() const override
    {
        auto process = get(10).value().value;
        return lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>>::success(
            {{process}, {}});
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
        std::vector<lx::SocketInfo> values;
        if (!query.localPort || *query.localPort == 8080) {
            lx::SocketInfo socket;
            socket.local = {"127.0.0.1", 8080};
            socket.inode = 1;
            values.push_back(socket);
        }
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success(
            {std::move(values), {}});
    }
};
class Owners final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>&) const override
    {
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {{{1, {10}}}, {}});
    }
};
class Services final : public lx::contracts::IServiceProvider {
public:
    lx::Result<void> probe() const override { return lx::Result<void>::success(); }
    lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>> list() const override
    { return lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>>::success({{}, {}}); }
    lx::Result<lx::Observation<lx::ServiceInfo>> get(const std::string& unit) const override
    {
        if (unit != "demo.service") return lx::Result<lx::Observation<lx::ServiceInfo>>::failure(
            {lx::ErrorCode::NotFound, "missing service", 0, "fake", "get"});
        lx::ServiceInfo service;
        service.unitName = unit;
        service.mainPid = 10;
        return lx::Result<lx::Observation<lx::ServiceInfo>>::success({service, {}});
    }
    lx::Result<std::optional<std::string>> unitByPid(pid_t pid) const override
    { return lx::Result<std::optional<std::string>>::success(pid == 10 ? std::optional<std::string>{"demo.service"} : std::nullopt); }
    lx::Result<void> start(const std::string&) const override { return lx::Result<void>::success(); }
    lx::Result<void> stop(const std::string&) const override { return lx::Result<void>::success(); }
    lx::Result<void> restart(const std::string&) const override { return lx::Result<void>::success(); }
};
class Journal final : public lx::contracts::IJournalProvider {
public:
    lx::Result<void> probe() const override { return lx::Result<void>::success(); }
    lx::Result<lx::Observation<std::vector<lx::JournalEntry>>> query(
        const lx::LogQuery& query) const override
    {
        lx::JournalEntry entry;
        entry.cursor = query.unit ? *query.unit : std::to_string(*query.pid);
        entry.systemdUnit = query.unit;
        entry.pid = query.pid;
        entry.message = "ready";
        return lx::Result<lx::Observation<std::vector<lx::JournalEntry>>>::success(
            {{entry}, {}});
    }
    lx::Result<void> follow(const lx::LogQuery&,
                            const lx::contracts::JournalEntrySink&,
                            const lx::contracts::JournalStopRequested&) const override
    { return lx::Result<void>::success(); }
};

struct Fixture {
    Processes processes;
    Signals signals;
    Services services;
    lx::application::ServiceService serviceService{services};
    lx::application::ProcessService processService{processes, signals, 999, &serviceService};
    Sockets sockets;
    Owners owners;
    lx::application::PortService portService{sockets, owners, processService, &serviceService};
    Journal journal;
    lx::application::LogService logService{journal};
    lx::application::ResourceResolver resolver{portService, processService, serviceService};
    lx::application::InspectService inspect{resolver, portService, processService, serviceService, logService};
};
} // namespace

TEST_CASE("InspectService builds port process service and log relationships")
{
    Fixture fixture;
    const auto result = fixture.inspect.inspect(lx::PortTarget{8080});
    REQUIRE(result);
    CHECK(result.value().ports.size() == 1);
    CHECK(result.value().processes.size() == 1);
    CHECK(result.value().services.size() == 1);
    CHECK(result.value().recentLogs.size() == 1);
}

TEST_CASE("InspectService builds PID and service rooted graphs")
{
    Fixture fixture;
    const auto process = fixture.inspect.inspect(lx::ProcessTarget{10});
    REQUIRE(process);
    CHECK(process.value().ports.size() == 1);
    CHECK(process.value().services.size() == 1);
    CHECK(process.value().recentLogs.front().pid == 10);

    const auto service = fixture.inspect.inspect(lx::ServiceTarget{"demo.service"});
    REQUIRE(service);
    CHECK(service.value().processes.size() == 1);
    CHECK(service.value().ports.size() == 1);
    CHECK(service.value().recentLogs.front().systemdUnit == "demo.service");
}

TEST_CASE("InspectService treats a missing root resource as an error")
{
    Fixture fixture;
    const auto result = fixture.inspect.inspect(lx::PortTarget{9999});
    REQUIRE_FALSE(result);
    CHECK(result.error().code == lx::ErrorCode::NotFound);
}
