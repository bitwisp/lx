#include "lx/application/PortService.h"

#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <tuple>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace lx::application {
namespace {

Warning warningFrom(const Error& error)
{
    return {error.code, error.message, error.systemError, error.component,
            error.operation};
}

using SocketIdentity = std::tuple<TransportProtocol, AddressFamily, std::string,
                                  std::uint16_t, std::uint64_t,
                                  std::vector<pid_t>>;

std::vector<SocketIdentity> identities(const PortReleasePlan& plan)
{
    std::vector<SocketIdentity> values;
    values.reserve(plan.ports.size());
    for (const auto& port : plan.ports) {
        values.emplace_back(
            port.socket.protocol, port.socket.family, port.socket.local.address,
            port.socket.local.port, port.socket.inode, port.socket.ownerPids);
    }
    std::sort(values.begin(), values.end());
    return values;
}

bool sameTargets(const PortReleasePlan& expected,
                 const PortReleasePlan& current)
{
    return identities(expected) == identities(current);
}

bool remainingTargetsBelongTo(
    const PortReleasePlan& original, const PortReleasePlan& current)
{
    for (const auto& currentPort : current.ports) {
        const auto found = std::find_if(
            original.ports.begin(), original.ports.end(),
            [&currentPort](const PortInfo& candidate) {
                return candidate.socket.protocol == currentPort.socket.protocol &&
                       candidate.socket.family == currentPort.socket.family &&
                       candidate.socket.local.address ==
                           currentPort.socket.local.address &&
                       candidate.socket.local.port ==
                           currentPort.socket.local.port &&
                       candidate.socket.inode == currentPort.socket.inode;
            });
        if (found == original.ports.end() ||
            !std::includes(found->socket.ownerPids.begin(),
                           found->socket.ownerPids.end(),
                           currentPort.socket.ownerPids.begin(),
                           currentPort.socket.ownerPids.end())) {
            return false;
        }
    }
    return true;
}

void appendWarnings(std::vector<Warning>& target, std::vector<Warning>& source)
{
    target.insert(target.end(), std::make_move_iterator(source.begin()),
                  std::make_move_iterator(source.end()));
}

Error changedTargetError()
{
    return {ErrorCode::Conflict,
            "Port ownership changed; refusing to signal a new target", 0,
            "port-service", "release"};
}

} // namespace

PortService::PortService(
    const contracts::ISocketProvider& socketProvider,
    const contracts::ISocketOwnerResolver& ownerResolver,
    const ProcessService& processService,
    const std::chrono::milliseconds gracePeriod) noexcept
    : socketProvider_(socketProvider), ownerResolver_(ownerResolver),
      processService_(processService), serviceService_(nullptr),
      gracePeriod_(gracePeriod)
{
}

PortService::PortService(
    const contracts::ISocketProvider& socketProvider,
    const contracts::ISocketOwnerResolver& ownerResolver,
    const ProcessService& processService,
    const ServiceService* serviceService,
    const std::chrono::milliseconds gracePeriod) noexcept
    : socketProvider_(socketProvider), ownerResolver_(ownerResolver),
      processService_(processService), serviceService_(serviceService),
      gracePeriod_(gracePeriod)
{
}

Result<Observation<std::vector<PortInfo>>> PortService::inspect(
    const SocketQuery& query) const
{
    auto sockets = socketProvider_.query(query);
    if (!sockets) {
        return Result<Observation<std::vector<PortInfo>>>::failure(
            sockets.error());
    }

    std::vector<std::uint64_t> inodes;
    inodes.reserve(sockets.value().value.size());
    for (const auto& socket : sockets.value().value) {
        if (socket.inode != 0) inodes.push_back(socket.inode);
    }

    auto ownership = ownerResolver_.resolve(inodes);
    if (!ownership) {
        return Result<Observation<std::vector<PortInfo>>>::failure(
            ownership.error());
    }

    auto warnings = std::move(sockets.value().warnings);
    warnings.insert(warnings.end(),
                    std::make_move_iterator(ownership.value().warnings.begin()),
                    std::make_move_iterator(ownership.value().warnings.end()));

    std::unordered_set<pid_t> ownerPids;
    for (auto& socket : sockets.value().value) {
        const auto found = ownership.value().value.find(socket.inode);
        if (found == ownership.value().value.end()) continue;
        socket.ownerPids = found->second;
        ownerPids.insert(found->second.begin(), found->second.end());
    }

    std::unordered_map<pid_t, ProcessInfo> processes;
    for (const auto pid : ownerPids) {
        auto process = processService_.inspect(pid);
        if (!process) {
            warnings.push_back(warningFrom(process.error()));
            continue;
        }
        warnings.insert(
            warnings.end(),
            std::make_move_iterator(process.value().warnings.begin()),
            std::make_move_iterator(process.value().warnings.end()));
        processes.emplace(pid, std::move(process.value().value));
    }

    std::vector<PortInfo> ports;
    ports.reserve(sockets.value().value.size());
    for (auto& socket : sockets.value().value) {
        PortInfo port;
        for (const auto pid : socket.ownerPids) {
            const auto process = processes.find(pid);
            if (process != processes.end()) port.owners.push_back(process->second);
        }
        port.socket = std::move(socket);
        ports.push_back(std::move(port));
    }
    return Result<Observation<std::vector<PortInfo>>>::success(
        {std::move(ports), std::move(warnings)});
}

Result<Observation<PortReleasePlan>> PortService::prepareRelease(
    const std::uint16_t localPort) const
{
    if (localPort == 0) {
        return Result<Observation<PortReleasePlan>>::failure({
            ErrorCode::InvalidArgument, "Port must be between 1 and 65535", 0,
            "port-service", "prepare release"});
    }

    SocketQuery query;
    query.localPort = localPort;
    auto inspected = inspect(query);
    if (!inspected) {
        return Result<Observation<PortReleasePlan>>::failure(inspected.error());
    }
    if (inspected.value().value.empty()) {
        return Result<Observation<PortReleasePlan>>::failure({
            ErrorCode::NotFound,
            "No socket found for port " + std::to_string(localPort), 0,
            "port-service", "prepare release"});
    }

    PortReleasePlan plan;
    plan.localPort = localPort;
    plan.ports = std::move(inspected.value().value);
    for (const auto& port : plan.ports) {
        if (port.socket.ownerPids.empty()) {
            return Result<Observation<PortReleasePlan>>::failure({
                ErrorCode::PermissionDenied,
                "Cannot safely release the port because a socket owner is unresolved",
                0, "port-service", "prepare release"});
        }
        plan.ownerPids.insert(plan.ownerPids.end(),
                              port.socket.ownerPids.begin(),
                              port.socket.ownerPids.end());
    }
    std::sort(plan.ownerPids.begin(), plan.ownerPids.end());
    plan.ownerPids.erase(
        std::unique(plan.ownerPids.begin(), plan.ownerPids.end()),
        plan.ownerPids.end());

    std::optional<std::string> commonUnit;
    bool managedByOneService = !plan.ownerPids.empty();
    for (const auto pid : plan.ownerPids) {
        const ProcessInfo* owner = nullptr;
        for (const auto& port : plan.ports) {
            const auto found = std::find_if(
                port.owners.begin(), port.owners.end(),
                [pid](const ProcessInfo& process) { return process.pid == pid; });
            if (found != port.owners.end()) {
                owner = &*found;
                break;
            }
        }
        if (owner == nullptr || !owner->systemdUnit) {
            managedByOneService = false;
            break;
        }
        if (!commonUnit) commonUnit = owner->systemdUnit;
        else if (*commonUnit != *owner->systemdUnit) {
            managedByOneService = false;
            break;
        }
    }
    if (managedByOneService) plan.recommendedUnit = std::move(commonUnit);

    for (const auto pid : plan.ownerPids) {
        const auto valid = processService_.validateSignalTarget(pid);
        if (!valid) {
            return Result<Observation<PortReleasePlan>>::failure(valid.error());
        }
    }
    return Result<Observation<PortReleasePlan>>::success(
        {std::move(plan), std::move(inspected.value().warnings)});
}

Result<Observation<PortReleaseResult>> PortService::stopManagedService(
    const PortReleasePlan& plan) const
{
    if (!plan.recommendedUnit || serviceService_ == nullptr) {
        return Result<Observation<PortReleaseResult>>::failure(
            {ErrorCode::InvalidArgument,
             "Port release plan has no managed service", 0, "port-service",
             "stop managed service"});
    }

    auto current = prepareRelease(plan.localPort);
    if (!current) {
        if (current.error().code == ErrorCode::NotFound) {
            return Result<Observation<PortReleaseResult>>::success(
                {{true, {}, std::nullopt}, {}});
        }
        return Result<Observation<PortReleaseResult>>::failure(current.error());
    }
    if (!sameTargets(plan, current.value().value) ||
        current.value().value.recommendedUnit != plan.recommendedUnit) {
        return Result<Observation<PortReleaseResult>>::failure(
            changedTargetError());
    }

    auto stopped = serviceService_->stop(*plan.recommendedUnit);
    if (!stopped) {
        return Result<Observation<PortReleaseResult>>::failure(stopped.error());
    }

    auto warnings = std::move(current.value().warnings);
    const auto deadline = std::chrono::steady_clock::now() + gracePeriod_;
    while (true) {
        auto after = prepareRelease(plan.localPort);
        if (!after) {
            if (after.error().code == ErrorCode::NotFound) {
                return Result<Observation<PortReleaseResult>>::success(
                    {{true, {}, std::nullopt}, std::move(warnings)});
            }
            return Result<Observation<PortReleaseResult>>::failure(after.error());
        }
        appendWarnings(warnings, after.value().warnings);
        if (!remainingTargetsBelongTo(plan, after.value().value)) {
            return Result<Observation<PortReleaseResult>>::failure(
                changedTargetError());
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return Result<Observation<PortReleaseResult>>::failure(
                {ErrorCode::Timeout,
                 "Port is still occupied after stopping " +
                     *plan.recommendedUnit,
                 0, "port-service", "stop managed service"});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
}

Result<Observation<PortReleaseResult>> PortService::terminate(
    const PortReleasePlan& plan) const
{
    auto current = prepareRelease(plan.localPort);
    if (!current) {
        if (current.error().code == ErrorCode::NotFound) {
            return Result<Observation<PortReleaseResult>>::success(
                {{true, {}, std::nullopt}, {}});
        }
        return Result<Observation<PortReleaseResult>>::failure(current.error());
    }
    if (!sameTargets(plan, current.value().value)) {
        return Result<Observation<PortReleaseResult>>::failure(
            changedTargetError());
    }

    PortReleaseResult result;
    for (const auto pid : plan.ownerPids) {
        auto delivery = processService_.stop(pid);
        if (!delivery) {
            auto error = delivery.error();
            if (!result.deliveries.empty()) {
                error.message = "Port release was partially signaled: " +
                                error.message;
            }
            return Result<Observation<PortReleaseResult>>::failure(
                std::move(error));
        }
        result.deliveries.push_back(std::move(delivery).value());
    }

    const auto deadline = std::chrono::steady_clock::now() + gracePeriod_;
    for (const auto pid : plan.ownerPids) {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = now >= deadline
                                   ? std::chrono::milliseconds{0}
                                   : std::chrono::duration_cast<
                                         std::chrono::milliseconds>(deadline - now);
        const auto exited = processService_.waitForExit(pid, remaining);
        if (!exited) {
            return Result<Observation<PortReleaseResult>>::failure(
                exited.error());
        }
    }

    auto after = prepareRelease(plan.localPort);
    auto warnings = std::move(current.value().warnings);
    if (!after) {
        if (after.error().code == ErrorCode::NotFound) {
            result.released = true;
            return Result<Observation<PortReleaseResult>>::success(
                {std::move(result), std::move(warnings)});
        }
        return Result<Observation<PortReleaseResult>>::failure(after.error());
    }
    appendWarnings(warnings, after.value().warnings);
    if (!remainingTargetsBelongTo(plan, after.value().value)) {
        return Result<Observation<PortReleaseResult>>::failure(
            changedTargetError());
    }
    result.remaining = std::move(after.value().value);
    return Result<Observation<PortReleaseResult>>::success(
        {std::move(result), std::move(warnings)});
}

Result<Observation<PortReleaseResult>> PortService::force(
    const PortReleasePlan& plan) const
{
    auto current = prepareRelease(plan.localPort);
    if (!current) {
        if (current.error().code == ErrorCode::NotFound) {
            return Result<Observation<PortReleaseResult>>::success(
                {{true, {}, std::nullopt}, {}});
        }
        return Result<Observation<PortReleaseResult>>::failure(current.error());
    }
    if (!sameTargets(plan, current.value().value)) {
        return Result<Observation<PortReleaseResult>>::failure(
            changedTargetError());
    }

    PortReleaseResult result;
    for (const auto pid : plan.ownerPids) {
        auto delivery = processService_.kill(pid);
        if (!delivery) {
            auto error = delivery.error();
            if (!result.deliveries.empty()) {
                error.message = "Port release was partially signaled: " +
                                error.message;
            }
            return Result<Observation<PortReleaseResult>>::failure(
                std::move(error));
        }
        result.deliveries.push_back(std::move(delivery).value());
    }
    const auto deadline = std::chrono::steady_clock::now() + gracePeriod_;
    for (const auto pid : plan.ownerPids) {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = now >= deadline
                                   ? std::chrono::milliseconds{0}
                                   : std::chrono::duration_cast<
                                         std::chrono::milliseconds>(deadline - now);
        const auto exited = processService_.waitForExit(pid, remaining);
        if (!exited) {
            return Result<Observation<PortReleaseResult>>::failure(
                exited.error());
        }
    }
    const auto after = prepareRelease(plan.localPort);
    if (!after && after.error().code == ErrorCode::NotFound) {
        result.released = true;
        return Result<Observation<PortReleaseResult>>::success(
            {std::move(result), std::move(current.value().warnings)});
    }
    if (!after) {
        return Result<Observation<PortReleaseResult>>::failure(after.error());
    }
    if (!remainingTargetsBelongTo(plan, after.value().value)) {
        return Result<Observation<PortReleaseResult>>::failure(
            changedTargetError());
    }
    return Result<Observation<PortReleaseResult>>::failure({
        ErrorCode::Timeout,
        "Port is still occupied after SIGKILL", 0,
        "port-service", "force release"});
}
} // namespace lx::application
