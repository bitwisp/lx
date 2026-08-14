#include "lx/domain/Result.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

lx::Error sampleError()
{
    return {
        lx::ErrorCode::PermissionDenied,
        "kernel denied the operation",
        13,
        "signal-provider",
        "send",
    };
}

} // namespace

TEST_CASE("Result contains a successful value")
{
    auto result = lx::Result<std::string>::success("ready");

    REQUIRE(result.hasValue());
    REQUIRE(static_cast<bool>(result));
    REQUIRE(result.value() == "ready");
    REQUIRE_THROWS_AS(result.error(), std::logic_error);
}

TEST_CASE("Result preserves structured error context")
{
    const auto result = lx::Result<int>::failure(sampleError());

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.error().code == lx::ErrorCode::PermissionDenied);
    REQUIRE(result.error().message == "kernel denied the operation");
    REQUIRE(result.error().systemError == 13);
    REQUIRE(result.error().component == "signal-provider");
    REQUIRE(result.error().operation == "send");
    REQUIRE_THROWS_AS(result.value(), std::logic_error);
}

TEST_CASE("Result moves unique ownership")
{
    auto result = lx::Result<std::unique_ptr<int>>::success(
        std::make_unique<int>(42));

    auto value = std::move(result).value();

    REQUIRE(value);
    REQUIRE(*value == 42);
}

TEST_CASE("Result void represents success and failure")
{
    const auto success = lx::Result<void>::success();
    REQUIRE(success.hasValue());
    REQUIRE_NOTHROW(success.value());
    REQUIRE_THROWS_AS(success.error(), std::logic_error);

    const auto failure = lx::Result<void>::failure(sampleError());
    REQUIRE_FALSE(failure.hasValue());
    REQUIRE(failure.error().code == lx::ErrorCode::PermissionDenied);
    REQUIRE_THROWS_AS(failure.value(), std::logic_error);
}

