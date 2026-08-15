#include "lx/domain/SocketInfo.h"
#include <catch2/catch_test_macros.hpp>
TEST_CASE("socket model permits shared ownership") {
    lx::SocketInfo socket;
    socket.ownerPids = {10, 20};
    REQUIRE(socket.ownerPids.size() == 2);
}
