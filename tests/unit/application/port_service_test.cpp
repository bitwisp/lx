#include "lx/application/PortService.h"

#include <catch2/catch_test_macros.hpp>

#include <unordered_map>
#include <utility>

namespace {

class SocketProvider final : public lx::contracts::ISocketProvider {
public:
    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(
        const lx::SocketQuery&) const override
    {
        lx::SocketInfo first;
        first.inode = 42;
        lx::SocketInfo second;
        second.inode = 84;
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success(
            {{first, second},
             {{lx::ErrorCode::IoError, "socket warning", 0, "fake-socket",
               "query"}}});
    }
};

class OwnerResolver final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>& inodes) const override
    {
        requested = inodes;
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {{{42, {10, 20}}, {84, {10}}},
             {{lx::ErrorCode::PermissionDenied, "owner warning", 0,
               "fake-owner", "resolve"}}});
    }

    mutable std::vector<std::uint64_t> requested;
};

class ProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(
        const pid_t pid) const override
    {
        ++calls[pid];
        if (pid == 20) {
            return lx::Result<lx::Observation<lx::ProcessInfo>>::failure({
                lx::ErrorCode::NotFound, "process disappeared", 0,
                "fake-process", "get"});
        }
        lx::ProcessInfo info;
        info.pid = pid;
        info.name = "demo";
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(info), {}});
    }

    mutable std::unordered_map<pid_t, int> calls;
};

} // namespace

TEST_CASE("PortService enriches sockets with shared process owners")
{
    const SocketProvider sockets;
    const OwnerResolver owners;
    const ProcessProvider processes;
    const lx::application::PortService service{sockets, owners, processes};

    const auto result = service.inspect({});

    REQUIRE(result);
    REQUIRE(owners.requested == std::vector<std::uint64_t>{42, 84});
    REQUIRE(result.value().value[0].socket.ownerPids ==
            std::vector<pid_t>{10, 20});
    REQUIRE(result.value().value[0].owners.size() == 1);
    REQUIRE(result.value().value[0].owners.front().name == "demo");
    REQUIRE(result.value().value[1].owners.size() == 1);
    REQUIRE(processes.calls.at(10) == 1);
    REQUIRE(processes.calls.at(20) == 1);
    REQUIRE(result.value().warnings.size() == 3);
}
