#include "lx/linux/systemd/SystemdServiceProvider.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <unistd.h>

TEST_CASE("systemd provider performs non-mutating system bus queries")
{
    const lx::linux::SystemdServiceProvider provider;
    const auto probe = provider.probe();
    if (!probe) {
        SKIP("systemd manager unavailable: " + probe.error().message);
    }

    const auto services = provider.list();
    REQUIRE(services);
    if (!services.value().value.empty()) {
        const auto detail = provider.get(
            services.value().value.front().unitName);
        REQUIRE(detail);
        REQUIRE(detail.value().value.unitName ==
                services.value().value.front().unitName);
    }

    const auto currentProcess = provider.unitByPid(::getpid());
    REQUIRE(currentProcess);
}
