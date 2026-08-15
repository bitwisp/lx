#include "lx/application/InspectService.h"

#include "lx/application/LogService.h"
#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ResourceResolver.h"
#include "lx/application/ServiceService.h"

#include <algorithm>
#include <set>
#include <type_traits>

namespace lx::application {
namespace {

Warning warningFrom(const Error& error)
{
    return {error.code, error.message, error.systemError, error.component,
            error.operation};
}

template <typename T>
void appendWarnings(ResourceGraph& graph, const Observation<T>& observed)
{
    graph.warnings.insert(graph.warnings.end(), observed.warnings.begin(),
                          observed.warnings.end());
}

void addProcess(ResourceGraph& graph, const ProcessInfo& process)
{
    const auto found = std::find_if(
        graph.processes.begin(), graph.processes.end(),
        [&process](const ProcessInfo& value) { return value.pid == process.pid; });
    if (found == graph.processes.end()) graph.processes.push_back(process);
}

void addService(ResourceGraph& graph, const ServiceInfo& service)
{
    const auto found = std::find_if(
        graph.services.begin(), graph.services.end(),
        [&service](const ServiceInfo& value) {
            return value.unitName == service.unitName;
        });
    if (found == graph.services.end()) graph.services.push_back(service);
}

void addLogs(ResourceGraph& graph, const LogService& service, LogQuery query)
{
    query.limit = 20;
    auto logs = service.read(std::move(query));
    if (!logs) {
        graph.warnings.push_back(warningFrom(logs.error()));
        return;
    }
    appendWarnings(graph, logs.value());
    for (auto& entry : logs.value().value) {
        if (!entry.cursor.empty()) {
            const auto duplicate = std::find_if(
                graph.recentLogs.begin(), graph.recentLogs.end(),
                [&entry](const JournalEntry& value) {
                    return value.cursor == entry.cursor;
                });
            if (duplicate != graph.recentLogs.end()) continue;
        }
        graph.recentLogs.push_back(std::move(entry));
    }
}

void finalizeLogs(ResourceGraph& graph)
{
    std::stable_sort(
        graph.recentLogs.begin(), graph.recentLogs.end(),
        [](const JournalEntry& left, const JournalEntry& right) {
            return left.timestamp < right.timestamp;
        });
    constexpr std::size_t limit = 20;
    if (graph.recentLogs.size() > limit) {
        graph.recentLogs.erase(
            graph.recentLogs.begin(), graph.recentLogs.end() - limit);
    }
}

} // namespace

InspectService::InspectService(
    const ResourceResolver& resolver, const PortService& portService,
    const ProcessService& processService,
    const ServiceService& serviceService,
    const LogService& logService) noexcept
    : resolver_(resolver), portService_(portService),
      processService_(processService), serviceService_(serviceService),
      logService_(logService)
{
}

Result<ResourceGraph> InspectService::inspect(
    const std::string& expression) const
{
    auto target = resolver_.resolve(expression);
    if (!target) return Result<ResourceGraph>::failure(target.error());
    return inspect(std::move(target).value());
}

Result<ResourceGraph> InspectService::inspect(ResourceTarget target) const
{
    ResourceGraph graph;
    graph.root = target;

    const auto result = std::visit(
        [this, &graph](const auto& root) -> Result<void> {
            using Root = std::decay_t<decltype(root)>;
            if constexpr (std::is_same<Root, PortTarget>::value) {
                SocketQuery query;
                query.localPort = root.port;
                auto ports = portService_.inspect(query);
                if (!ports) return Result<void>::failure(ports.error());
                if (ports.value().value.empty()) {
                    return Result<void>::failure({
                        ErrorCode::NotFound,
                        "No socket found for port " + std::to_string(root.port),
                        0, "inspect-service", "inspect port"});
                }
                appendWarnings(graph, ports.value());
                graph.ports = std::move(ports.value().value);
                std::set<std::string> units;
                std::set<pid_t> unmanagedPids;
                for (const auto& port : graph.ports) {
                    for (const auto& process : port.owners) {
                        addProcess(graph, process);
                        if (process.systemdUnit) units.insert(*process.systemdUnit);
                        else unmanagedPids.insert(process.pid);
                    }
                }
                for (const auto& unit : units) {
                    auto service = serviceService_.inspect(unit);
                    if (service) {
                        appendWarnings(graph, service.value());
                        addService(graph, service.value().value);
                    } else graph.warnings.push_back(warningFrom(service.error()));
                    LogQuery logs;
                    logs.unit = unit;
                    addLogs(graph, logService_, std::move(logs));
                }
                for (const auto pid : unmanagedPids) {
                    LogQuery logs;
                    logs.pid = pid;
                    addLogs(graph, logService_, std::move(logs));
                }
            } else if constexpr (std::is_same<Root, ProcessTarget>::value) {
                auto process = processService_.inspect(root.pid);
                if (!process) return Result<void>::failure(process.error());
                appendWarnings(graph, process.value());
                addProcess(graph, process.value().value);
                if (process.value().value.systemdUnit) {
                    auto service = serviceService_.inspect(
                        *process.value().value.systemdUnit);
                    if (service) {
                        appendWarnings(graph, service.value());
                        addService(graph, service.value().value);
                    } else graph.warnings.push_back(warningFrom(service.error()));
                }
                SocketQuery query;
                auto ports = portService_.inspect(query);
                if (ports) {
                    appendWarnings(graph, ports.value());
                    for (auto& port : ports.value().value) {
                        if (std::find(port.socket.ownerPids.begin(),
                                      port.socket.ownerPids.end(), root.pid) !=
                            port.socket.ownerPids.end()) {
                            graph.ports.push_back(std::move(port));
                        }
                    }
                } else graph.warnings.push_back(warningFrom(ports.error()));
                LogQuery logs;
                logs.pid = root.pid;
                addLogs(graph, logService_, std::move(logs));
            } else {
                auto service = serviceService_.inspect(root.unit);
                if (!service) return Result<void>::failure(service.error());
                appendWarnings(graph, service.value());
                addService(graph, service.value().value);
                ProcessQuery processesQuery;
                processesQuery.systemdUnit = root.unit;
                auto processes = processService_.list(std::move(processesQuery));
                std::set<pid_t> pids;
                if (processes) {
                    appendWarnings(graph, processes.value());
                    graph.processes = std::move(processes.value().value);
                    for (const auto& process : graph.processes) pids.insert(process.pid);
                } else graph.warnings.push_back(warningFrom(processes.error()));
                SocketQuery query;
                auto ports = portService_.inspect(query);
                if (ports) {
                    appendWarnings(graph, ports.value());
                    for (auto& port : ports.value().value) {
                        if (std::any_of(
                                port.socket.ownerPids.begin(),
                                port.socket.ownerPids.end(),
                                [&pids](const pid_t pid) {
                                    return pids.count(pid) != 0;
                                })) {
                            graph.ports.push_back(std::move(port));
                        }
                    }
                } else graph.warnings.push_back(warningFrom(ports.error()));
                LogQuery logs;
                logs.unit = root.unit;
                addLogs(graph, logService_, std::move(logs));
            }
            return Result<void>::success();
        },
        target);
    if (!result) return Result<ResourceGraph>::failure(result.error());
    finalizeLogs(graph);
    return Result<ResourceGraph>::success(std::move(graph));
}

} // namespace lx::application
