#include "lx/linux/process/PidFd.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <utility>
#include <unistd.h>

TEST_CASE("PidFd owns a movable process handle when supported")
{
    auto opened = lx::linux::process::PidFd::open(::getpid());
    if (!opened && opened.error().code == lx::ErrorCode::Unsupported) {
        SKIP("pidfd is unavailable on this kernel");
    }
    REQUIRE(opened);

    auto handle = std::move(opened).value();
    REQUIRE(handle.valid());
    auto moved = std::move(handle);
    REQUIRE_FALSE(handle.valid());
    REQUIRE(moved.valid());

    const auto exited = moved.waitForExit(std::chrono::milliseconds{0});
    REQUIRE(exited);
    REQUIRE_FALSE(exited.value());
}
