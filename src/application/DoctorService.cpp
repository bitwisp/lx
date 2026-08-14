#include "lx/application/DoctorService.h"

#include <unistd.h>

namespace lx::application {

DoctorService::DoctorService(const contracts::IProcessProvider& processProvider) noexcept
    : processProvider_(processProvider)
{
}

DoctorReport DoctorService::inspect() const
{
    DoctorReport report{{
        {"Project foundation", CapabilityStatus::Available,
         "C++17 application and test infrastructure are ready"},
        {"Process API", CapabilityStatus::Unavailable, "not checked"},
        {"Socket API", CapabilityStatus::NotImplemented,
         "INET_DIAG provider is planned for Phase 2"},
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
    return report;
}

} // namespace lx::application
