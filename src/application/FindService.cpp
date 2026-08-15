#include "lx/application/FindService.h"

#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace lx::application {
namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool contains(const std::string& value, const std::string& needle)
{
    return lower(value).find(needle) != std::string::npos;
}

Warning warningFrom(const Error& error)
{
    return {error.code, error.message, error.systemError, error.component,
            error.operation};
}

bool processMatches(const ProcessInfo& process, const std::string& query)
{
    if (contains(process.name, query) || contains(process.user, query) ||
        (process.executable && contains(*process.executable, query))) {
        return true;
    }
    return std::any_of(process.argv.begin(), process.argv.end(),
                       [&query](const std::string& argument) {
                           return contains(argument, query);
                       });
}

} // namespace

FindService::FindService(
    const PortService& portService, const ProcessService& processService,
    const ServiceService& serviceService) noexcept
    : portService_(portService), processService_(processService),
      serviceService_(serviceService)
{
}

Result<FindResult> FindService::find(const std::string& query) const
{
    const auto normalized = lower(query);
    if (normalized.empty()) {
        return Result<FindResult>::failure({
            ErrorCode::InvalidArgument, "Find query must not be empty", 0,
            "find-service", "find"});
    }

    FindResult result;
    std::set<std::string> matchedUnits;
    auto services = serviceService_.list();
    if (services) {
        result.warnings.insert(result.warnings.end(),
                               services.value().warnings.begin(),
                               services.value().warnings.end());
        for (auto& service : services.value().value) {
            if (contains(service.unitName, normalized) ||
                contains(service.description, normalized)) {
                matchedUnits.insert(service.unitName);
                result.services.push_back(std::move(service));
            }
        }
    } else {
        result.warnings.push_back(warningFrom(services.error()));
    }

    auto processes = processService_.list();
    if (!processes) return Result<FindResult>::failure(processes.error());
    result.warnings.insert(result.warnings.end(),
                           processes.value().warnings.begin(),
                           processes.value().warnings.end());
    std::set<pid_t> matchedPids;
    std::set<std::string> executables;
    for (auto& process : processes.value().value) {
        if (!processMatches(process, normalized) &&
            (!process.systemdUnit ||
             matchedUnits.count(*process.systemdUnit) == 0)) {
            continue;
        }
        matchedPids.insert(process.pid);
        if (process.executable) executables.insert(*process.executable);
        result.processes.push_back(std::move(process));
    }
    result.executables.assign(executables.begin(), executables.end());

    SocketQuery socketsQuery;
    auto ports = portService_.inspect(socketsQuery);
    if (ports) {
        result.warnings.insert(result.warnings.end(),
                               ports.value().warnings.begin(),
                               ports.value().warnings.end());
        for (auto& port : ports.value().value) {
            const bool direct = contains(
                std::to_string(port.socket.local.port), normalized);
            const bool relatedPid = std::any_of(
                port.socket.ownerPids.begin(), port.socket.ownerPids.end(),
                [&matchedPids](const pid_t pid) {
                    return matchedPids.count(pid) != 0;
                });
            const bool relatedUnit = std::any_of(
                port.owners.begin(), port.owners.end(),
                [&matchedUnits](const ProcessInfo& owner) {
                    return owner.systemdUnit &&
                           matchedUnits.count(*owner.systemdUnit) != 0;
                });
            if (direct || relatedPid || relatedUnit) {
                result.ports.push_back(std::move(port));
            }
        }
    } else {
        result.warnings.push_back(warningFrom(ports.error()));
    }

    if (result.services.empty() && result.processes.empty() &&
        result.ports.empty() && result.executables.empty()) {
        return Result<FindResult>::failure({
            ErrorCode::NotFound,
            "No observable resource matched \"" + query + "\"", 0,
            "find-service", "find"});
    }
    return Result<FindResult>::success(std::move(result));
}

} // namespace lx::application
