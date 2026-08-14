#include "lx/application/ProcessService.h"

namespace lx::application {

ProcessService::ProcessService(const contracts::IProcessProvider& provider) noexcept
    : provider_(provider)
{
}

Result<Observation<ProcessInfo>> ProcessService::inspect(const pid_t pid) const
{
    if (pid <= 0) {
        return Result<Observation<ProcessInfo>>::failure({
            ErrorCode::InvalidArgument,
            "PID must be greater than zero",
            0,
            "process-service",
            "inspect",
        });
    }
    return provider_.get(pid);
}

} // namespace lx::application

