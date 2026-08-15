#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

namespace lx::application {

ProcessService::ProcessService(
    const contracts::IProcessProvider& processProvider,
    const contracts::ISignalProvider& signalProvider,
    const pid_t selfPid,
    const ServiceService* serviceService) noexcept
    : processProvider_(processProvider), signalProvider_(signalProvider),
      selfPid_(selfPid), serviceService_(serviceService)
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
    auto observed = processProvider_.get(pid);
    if (!observed || serviceService_ == nullptr) return observed;

    auto unit = serviceService_->unitByPid(pid);
    if (unit) {
        observed.value().value.systemdUnit = std::move(unit).value();
    } else if (unit.error().code != ErrorCode::Unavailable &&
               unit.error().code != ErrorCode::NotFound) {
        observed.value().warnings.push_back(
            {unit.error().code, unit.error().message, unit.error().systemError,
             unit.error().component, unit.error().operation});
    }
    return observed;
}

Result<SignalDelivery> ProcessService::stop(const pid_t pid) const
{
    return signal(pid, ProcessSignal::Terminate);
}

Result<SignalDelivery> ProcessService::kill(const pid_t pid) const
{
    return signal(pid, ProcessSignal::Kill);
}

Result<bool> ProcessService::waitForExit(
    const pid_t pid, const std::chrono::milliseconds timeout) const
{
    const auto valid = validateSignalTarget(pid);
    if (!valid) return Result<bool>::failure(valid.error());
    return signalProvider_.waitForExit(pid, timeout);
}

Result<void> ProcessService::validateSignalTarget(const pid_t pid) const
{
    if (pid <= 0) {
        return Result<void>::failure({
            ErrorCode::InvalidArgument, "PID must be greater than zero", 0,
            "process-service", "validate signal target"});
    }
    if (pid == 1) {
        return Result<void>::failure({
            ErrorCode::Conflict, "Refusing to signal PID 1", 0,
            "process-service", "validate signal target"});
    }
    if (pid == selfPid_) {
        return Result<void>::failure({
            ErrorCode::Conflict, "Refusing to signal the LX process", 0,
            "process-service", "validate signal target"});
    }
    return Result<void>::success();
}

SignalCapabilities ProcessService::signalCapabilities() const
{
    return signalProvider_.capabilities();
}

Result<SignalDelivery> ProcessService::signal(
    const pid_t pid, const ProcessSignal processSignal) const
{
    const auto valid = validateSignalTarget(pid);
    if (!valid) return Result<SignalDelivery>::failure(valid.error());
    return signalProvider_.send(pid, processSignal);
}

} // namespace lx::application
