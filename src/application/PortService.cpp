#include "lx/application/PortService.h"

#include <algorithm>
#include <iterator>
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

} // namespace

PortService::PortService(
    const contracts::ISocketProvider& socketProvider,
    const contracts::ISocketOwnerResolver& ownerResolver,
    const contracts::IProcessProvider& processProvider) noexcept
    : socketProvider_(socketProvider), ownerResolver_(ownerResolver),
      processProvider_(processProvider)
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
        auto process = processProvider_.get(pid);
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
} // namespace lx::application
