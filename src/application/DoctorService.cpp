#include "lx/application/DoctorService.h"

#include <unistd.h>

namespace lx::application {

DoctorService::DoctorService(
    const contracts::IProcessProvider& processProvider,
    const contracts::ISocketProvider& socketProvider,
    const contracts::ISignalProvider& signalProvider,
    const contracts::IServiceProvider& serviceProvider) noexcept
    : processProvider_(processProvider), socketProvider_(socketProvider),
      signalProvider_(signalProvider), serviceProvider_(serviceProvider)
{
}

DoctorReport DoctorService::inspect() const
{
    DoctorReport report{{
        {"Project foundation", CapabilityStatus::Available,
         "C++17 application and test infrastructure are ready"},
        {"Process API", CapabilityStatus::Unavailable, "not checked"},
        {"Socket API", CapabilityStatus::Unavailable, "not checked"},
        {"Process signals", CapabilityStatus::Unavailable, "not checked"},
        {"systemd", CapabilityStatus::Unavailable, "not checked"},
        {"Journal", CapabilityStatus::NotImplemented,
         "journal provider is planned for Phase 6"},
    }};
    const auto process = processProvider_.get(::getpid());
    auto& check = report.checks[1];
    if (process) {
        check.status = CapabilityStatus::Available;
        check.detail = "ProcFS process inspection is available";
    } else {
        check.detail = process.error().message;
    }
    SocketQuery socketQuery; socketQuery.protocol=TransportProtocol::Tcp;
    const auto sockets=socketProvider_.query(socketQuery); auto& socketCheck=report.checks[2];
    if(sockets){socketCheck.status=CapabilityStatus::Available;socketCheck.detail="INET_DIAG socket inspection is available";}else socketCheck.detail=sockets.error().message;
    const auto signalCapabilities = signalProvider_.capabilities();
    auto& signalCheck = report.checks[3];
    if (signalCapabilities.signalingAvailable) {
        signalCheck.status = CapabilityStatus::Available;
        signalCheck.detail = signalCapabilities.pidFdAvailable
                                 ? "Process signaling uses pidfd when possible"
                                 : "Process signaling uses kill(2) fallback";
    } else {
        signalCheck.detail = "Process signaling is unavailable";
    }
    const auto systemd = serviceProvider_.probe();
    auto& systemdCheck = report.checks[4];
    if (systemd) {
        systemdCheck.status = CapabilityStatus::Available;
        systemdCheck.detail = "systemd service management is available";
    } else {
        systemdCheck.detail = systemd.error().message;
    }
    return report;
}

} // namespace lx::application
