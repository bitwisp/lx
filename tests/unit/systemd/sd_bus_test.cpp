#include "lx/linux/systemd/SdBus.h"

#include <catch2/catch_test_macros.hpp>
#include <utility>

TEST_CASE("sd-bus wrappers safely own native resources")
{
    lx::linux::SdBusMessage message;
    lx::linux::SdBusMessage moved{std::move(message)};
    lx::linux::SdBusError error;

    REQUIRE(moved.get() == nullptr);
    REQUIRE(error.value().name == nullptr);
    REQUIRE(error.value().message == nullptr);
}

TEST_CASE("sd-bus system connection reports success or classified failure")
{
    auto connection = lx::linux::SdBusConnection::openSystem();
    if (connection) {
        REQUIRE(connection.value().get() != nullptr);
    } else {
        REQUIRE((connection.error().code == lx::ErrorCode::Unavailable ||
                 connection.error().code == lx::ErrorCode::PermissionDenied));
    }
}
