#include "lx/application/DoctorService.h"

#include <unistd.h>

namespace lx::application {

DoctorService::DoctorService(
    const contracts::IProcessProvider& processProvider,
    const contracts::ISocketProvider& socketProvider,
    const contracts::ISignalProvider& signalProvider) noexcept
    : processProvider_(processProvider), socketProvider_(socketProvider),
      signalProvider_(signalProvider)
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
        {"systemd", CapabilityStatus::NotImplemented,
         "systemd provider is planned for Phase 5"},
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
    return report;
}

} // namespace lx::application
