#include "lx/application/DoctorService.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DoctorService reports only implemented foundation as available")
{
    const auto report = lx::application::DoctorService{}.inspect();

    REQUIRE(report.checks.size() == 5);
    REQUIRE(report.checks.front().name == "Project foundation");
    REQUIRE(report.checks.front().status == lx::CapabilityStatus::Available);

    for (auto check = report.checks.begin() + 1; check != report.checks.end();
         ++check) {
        REQUIRE(check->status == lx::CapabilityStatus::NotImplemented);
        REQUIRE_FALSE(check->detail.empty());
    }
}

