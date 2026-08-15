#include "lx/domain/PortInfo.h"
#include "lx/domain/SocketOwnership.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("port information represents shared socket ownership")
{
    lx::SocketOwnership ownership{{42, {10, 20}}};

    lx::PortInfo port;
    port.socket.inode = 42;
    port.socket.ownerPids = ownership.at(42);
    lx::ProcessInfo firstOwner;
    firstOwner.pid = 10;
    lx::ProcessInfo secondOwner;
    secondOwner.pid = 20;
    port.owners = {firstOwner, secondOwner};

    REQUIRE(port.socket.ownerPids == std::vector<pid_t>{10, 20});
    REQUIRE(port.owners.size() == 2);
    REQUIRE(port.owners.front().pid == 10);
}
