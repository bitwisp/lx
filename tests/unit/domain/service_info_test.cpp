#include "lx/domain/ServiceInfo.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("service information preserves systemd state")
{
    lx::ServiceInfo service;
    service.unitName = "demo.service";
    service.description = "Demo service";
    service.loadState = "loaded";
    service.activeState = "active";
    service.subState = "running";
    service.unitFileState = "enabled";
    service.mainPid = 42;
    service.activeEnterTimestampUsec = 123456;

    REQUIRE(service.unitName == "demo.service");
    REQUIRE(service.activeState == "active");
    REQUIRE(service.subState == "running");
    REQUIRE(service.mainPid == 42);
    REQUIRE(service.activeEnterTimestampUsec == 123456);
}
