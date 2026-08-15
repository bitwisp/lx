#include "lx/application/DoctorService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class DoctorProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(const pid_t pid) const override
    {
        lx::ProcessInfo info;
        info.pid = pid;
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(info), {}});
    }
};
class DoctorSocketProvider final : public lx::contracts::ISocketProvider {
public:
    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(const lx::SocketQuery&) const override
    { return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success({{}, {}}); }
};
class DoctorSignalProvider final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(
        pid_t, lx::ProcessSignal) const override
    { return lx::Result<lx::SignalDelivery>::failure({}); }
    lx::Result<bool> waitForExit(
        pid_t, std::chrono::milliseconds) const override
    { return lx::Result<bool>::success(false); }
    lx::SignalCapabilities capabilities() const override { return {true, true}; }
};
class DoctorServiceProvider final : public lx::contracts::IServiceProvider {
public:
    lx::Result<void> probe() const override
    {
        if (available) return lx::Result<void>::success();
        return lx::Result<void>::failure(
            {lx::ErrorCode::Unavailable, "systemd manager is not running", 0,
             "fake-systemd", "probe"});
    }
    lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>> list()
        const override
    {
        return lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>>::success(
            {{}, {}});
    }
    lx::Result<lx::Observation<lx::ServiceInfo>> get(
        const std::string&) const override
    {
        return lx::Result<lx::Observation<lx::ServiceInfo>>::failure({});
    }
    lx::Result<std::optional<std::string>> unitByPid(pid_t) const override
    {
        return lx::Result<std::optional<std::string>>::success(std::nullopt);
    }
    lx::Result<void> start(const std::string&) const override
    {
        return lx::Result<void>::success();
    }
    lx::Result<void> stop(const std::string&) const override
    {
        return lx::Result<void>::success();
    }
    lx::Result<void> restart(const std::string&) const override
    {
        return lx::Result<void>::success();
    }

    bool available = true;
};

} // namespace

TEST_CASE("DoctorService reports implemented capabilities as available")
{
    const DoctorProcessProvider provider;
    const DoctorSocketProvider sockets;
    const DoctorSignalProvider signals;
    const DoctorServiceProvider services;
    const auto report = lx::application::DoctorService{
        provider, sockets, signals, services}.inspect();

    REQUIRE(report.checks.size() == 6);
    REQUIRE(report.checks.front().name == "Project foundation");
    REQUIRE(report.checks.front().status == lx::CapabilityStatus::Available);

    REQUIRE(report.checks[1].status == lx::CapabilityStatus::Available);
    REQUIRE(report.checks[2].status == lx::CapabilityStatus::Available);
    REQUIRE(report.checks[3].status == lx::CapabilityStatus::Available);
    REQUIRE(report.checks[3].detail.find("pidfd") != std::string::npos);
    REQUIRE(report.checks[4].status == lx::CapabilityStatus::Available);
    REQUIRE(report.checks[5].status == lx::CapabilityStatus::NotImplemented);
}

TEST_CASE("DoctorService reports an unavailable systemd manager")
{
    const DoctorProcessProvider processes;
    const DoctorSocketProvider sockets;
    const DoctorSignalProvider signals;
    DoctorServiceProvider services;
    services.available = false;

    const auto report = lx::application::DoctorService{
        processes, sockets, signals, services}.inspect();

    REQUIRE(report.checks[4].status == lx::CapabilityStatus::Unavailable);
    REQUIRE(report.checks[4].detail == "systemd manager is not running");
}
