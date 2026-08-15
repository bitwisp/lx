#include "lx/linux/systemd/SystemdMessageParser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ListUnits parser keeps loaded service units")
{
    const auto service = lx::linux::serviceFromUnitRecord(
        {"demo.service", "Demo", "loaded", "active", "running"});
    const auto scope = lx::linux::serviceFromUnitRecord(
        {"session.scope", "Session", "loaded", "active", "running"});

    REQUIRE(service);
    REQUIRE(service->unitName == "demo.service");
    REQUIRE(service->activeState == "active");
    REQUIRE_FALSE(scope);
}

TEST_CASE("ListUnits parser rejects an absent response")
{
    const auto parsed = lx::linux::parseListUnitsMessage(nullptr);
    REQUIRE_FALSE(parsed);
    REQUIRE(parsed.error().code == lx::ErrorCode::ProtocolError);
}
