#include "lx/application/ServiceService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class FakeServiceProvider final : public lx::contracts::IServiceProvider {
public:
    lx::Result<void> probe() const override
    {
        return lx::Result<void>::success();
    }

    lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>> list()
        const override
    {
        return lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>>::success(
            {{{"demo.service", "Demo", "loaded", "active", "running",
               "enabled", 42, 1}}, {}});
    }

    lx::Result<lx::Observation<lx::ServiceInfo>> get(
        const std::string& unit) const override
    {
        lastUnit = unit;
        lx::ServiceInfo service;
        service.unitName = unit;
        return lx::Result<lx::Observation<lx::ServiceInfo>>::success(
            {std::move(service), {}});
    }

    lx::Result<std::optional<std::string>> unitByPid(
        const pid_t) const override
    {
        return lx::Result<std::optional<std::string>>::success(
            std::string{"demo.service"});
    }

    lx::Result<void> start(const std::string& unit) const override
    {
        lastAction = "start";
        lastUnit = unit;
        return lx::Result<void>::success();
    }

    lx::Result<void> stop(const std::string& unit) const override
    {
        lastAction = "stop";
        lastUnit = unit;
        return lx::Result<void>::success();
    }

    lx::Result<void> restart(const std::string& unit) const override
    {
        lastAction = "restart";
        lastUnit = unit;
        return lx::Result<void>::success();
    }

    mutable std::string lastAction;
    mutable std::string lastUnit;
};

} // namespace

TEST_CASE("service service normalizes service unit names")
{
    REQUIRE(lx::application::ServiceService::normalizeUnit("nginx").value() ==
            "nginx.service");
    REQUIRE(lx::application::ServiceService::normalizeUnit("nginx.service")
                .value() == "nginx.service");
    REQUIRE_FALSE(lx::application::ServiceService::normalizeUnit("../nginx"));
    REQUIRE_FALSE(lx::application::ServiceService::normalizeUnit("bad unit"));
}

TEST_CASE("service service routes lifecycle actions to provider")
{
    FakeServiceProvider provider;
    const lx::application::ServiceService service{provider};

    REQUIRE(service.start("demo"));
    REQUIRE(provider.lastAction == "start");
    REQUIRE(provider.lastUnit == "demo.service");
    REQUIRE(service.stop("demo.service"));
    REQUIRE(provider.lastAction == "stop");
    REQUIRE(service.restart("demo"));
    REQUIRE(provider.lastAction == "restart");
}

TEST_CASE("service service validates before querying provider")
{
    FakeServiceProvider provider;
    const lx::application::ServiceService service{provider};

    const auto invalid = service.inspect("bad/service");
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().code == lx::ErrorCode::InvalidArgument);
    REQUIRE(provider.lastUnit.empty());
}
