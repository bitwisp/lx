#include "lx/application/FindService.h"
#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <catch2/catch_test_macros.hpp>

namespace {
class Processes final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(pid_t pid) const override
    {
        lx::ProcessInfo process;
        process.pid = pid;
        process.name = "DemoWorker";
        process.executable = "/usr/bin/demo";
        process.argv = {"demo", "--serve"};
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success({process, {}});
    }
    lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>> list() const override
    { return lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>>::success({{{get(10).value().value}}, {}}); }
};
class Signals final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(pid_t, lx::ProcessSignal) const override
    { return lx::Result<lx::SignalDelivery>::failure({}); }
    lx::Result<bool> waitForExit(pid_t, std::chrono::milliseconds) const override
    { return lx::Result<bool>::success(false); }
    lx::SignalCapabilities capabilities() const override { return {}; }
};
class Services final : public lx::contracts::IServiceProvider {
public:
    lx::Result<void> probe() const override { return lx::Result<void>::success(); }
    lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>> list() const override
    {
        lx::ServiceInfo service;
        service.unitName = "demo.service";
        service.description = "Demo Server";
        return lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>>::success({{{service}}, {}});
    }
    lx::Result<lx::Observation<lx::ServiceInfo>> get(const std::string&) const override
    { return lx::Result<lx::Observation<lx::ServiceInfo>>::failure({}); }
    lx::Result<std::optional<std::string>> unitByPid(pid_t) const override
    { return lx::Result<std::optional<std::string>>::success(std::string{"demo.service"}); }
    lx::Result<void> start(const std::string&) const override { return lx::Result<void>::success(); }
    lx::Result<void> stop(const std::string&) const override { return lx::Result<void>::success(); }
    lx::Result<void> restart(const std::string&) const override { return lx::Result<void>::success(); }
};
class Sockets final : public lx::contracts::ISocketProvider {
public:
    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(const lx::SocketQuery&) const override
    {
        lx::SocketInfo socket;
        socket.local.port = 8080;
        socket.inode = 1;
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success({{{socket}}, {}});
    }
};
class Owners final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(const std::vector<std::uint64_t>&) const override
    { return lx::Result<lx::Observation<lx::SocketOwnership>>::success({{{1, {10}}}, {}}); }
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
    lx::application::FindService find{portService, processService, serviceService};
};
} // namespace

TEST_CASE("FindService searches resources case insensitively and follows relationships")
{
    Fixture fixture;
    const auto result = fixture.find.find("DEMO");
    REQUIRE(result);
    CHECK(result.value().services.size() == 1);
    CHECK(result.value().processes.size() == 1);
    CHECK(result.value().ports.size() == 1);
    REQUIRE(result.value().executables.size() == 1);
    CHECK(result.value().executables.front() == "/usr/bin/demo");
}

TEST_CASE("FindService matches ports directly and reports empty searches")
{
    Fixture fixture;
    const auto port = fixture.find.find("8080");
    REQUIRE(port);
    CHECK(port.value().ports.size() == 1);
    CHECK_FALSE(fixture.find.find("missing"));
    CHECK(fixture.find.find("missing").error().code == lx::ErrorCode::NotFound);
    CHECK(fixture.find.find("").error().code == lx::ErrorCode::InvalidArgument);
}
