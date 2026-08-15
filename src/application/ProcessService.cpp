#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <algorithm>
#include <charconv>
#include <iterator>

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

Result<Observation<std::vector<ProcessInfo>>> ProcessService::list(
    ProcessQuery query) const
{
    if (query.name && query.name->empty()) {
        return Result<Observation<std::vector<ProcessInfo>>>::failure({
            ErrorCode::InvalidArgument, "Process name must not be empty", 0,
            "process-service", "list"});
    }
    if (query.user && query.user->empty()) {
        return Result<Observation<std::vector<ProcessInfo>>>::failure({
            ErrorCode::InvalidArgument, "Process user must not be empty", 0,
            "process-service", "list"});
    }
    if (query.systemdUnit) {
        if (serviceService_ == nullptr) {
            return Result<Observation<std::vector<ProcessInfo>>>::failure({
                ErrorCode::Unavailable, "Service association is unavailable", 0,
                "process-service", "list"});
        }
        auto normalized = ServiceService::normalizeUnit(*query.systemdUnit);
        if (!normalized) {
            return Result<Observation<std::vector<ProcessInfo>>>::failure(
                normalized.error());
        }
        query.systemdUnit = std::move(normalized).value();
    }

    auto observed = processProvider_.list();
    if (!observed) return observed;

    std::optional<std::uint32_t> requestedUid;
    if (query.user) {
        std::uint32_t uid = 0;
        const auto parsed = std::from_chars(
            query.user->data(), query.user->data() + query.user->size(), uid);
        if (parsed.ec == std::errc{} &&
            parsed.ptr == query.user->data() + query.user->size()) {
            requestedUid = uid;
        }
    }

    std::vector<ProcessInfo> matches;
    matches.reserve(observed.value().value.size());
    for (auto& process : observed.value().value) {
        if (query.name && process.name != *query.name) continue;
        if (query.user &&
            (!requestedUid || process.uid != *requestedUid) &&
            process.user != *query.user) {
            continue;
        }

        if (serviceService_ != nullptr) {
            auto unit = serviceService_->unitByPid(process.pid);
            if (unit) {
                process.systemdUnit = std::move(unit).value();
            } else if (query.systemdUnit &&
                       unit.error().code == ErrorCode::Unavailable) {
                return Result<Observation<std::vector<ProcessInfo>>>::failure(
                    unit.error());
            } else if (unit.error().code != ErrorCode::Unavailable &&
                       unit.error().code != ErrorCode::NotFound) {
                observed.value().warnings.push_back(
                    {unit.error().code, unit.error().message,
                     unit.error().systemError, unit.error().component,
                     unit.error().operation});
            }
        }
        if (query.systemdUnit && process.systemdUnit != query.systemdUnit) continue;
        matches.push_back(std::move(process));
    }
    observed.value().value = std::move(matches);
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
