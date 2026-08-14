#include "lx/domain/Observation.h"
#include "lx/domain/ProcessInfo.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Observation carries a value and non-fatal warnings")
{
    lx::ProcessInfo process;
    process.pid = 42;

    lx::Observation<lx::ProcessInfo> observation{
        process,
        {{lx::ErrorCode::PermissionDenied, "cwd is unreadable", 13,
          "procfs", "read cwd"}},
    };

    REQUIRE(observation.value.pid == 42);
    REQUIRE(observation.warnings.size() == 1);
    REQUIRE(observation.warnings.front().code ==
            lx::ErrorCode::PermissionDenied);
}
