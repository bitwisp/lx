#include "lx/application/DoctorService.h"

namespace lx::application {

DoctorReport DoctorService::inspect() const
{
    return {{
        {"Project foundation", CapabilityStatus::Available,
         "C++17 application and test infrastructure are ready"},
        {"Process API", CapabilityStatus::NotImplemented,
         "ProcFS provider is planned for Phase 1"},
        {"Socket API", CapabilityStatus::NotImplemented,
         "INET_DIAG provider is planned for Phase 2"},
        {"systemd", CapabilityStatus::NotImplemented,
         "systemd provider is planned for Phase 5"},
        {"Journal", CapabilityStatus::NotImplemented,
         "journal provider is planned for Phase 6"},
    }};
}

} // namespace lx::application

