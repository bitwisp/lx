#include "lx/cli/JsonSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

TEST_CASE("JSON serializer creates stable success and error envelopes")
{
    const auto success = nlohmann::json::parse(
        lx::cli::JsonSerializer::emptySuccess("doctor", "inspect"));
    CHECK(success.at("schema_version") == 1);
    CHECK(success.at("command") == "doctor");
    CHECK(success.at("operation") == "inspect");
    CHECK(success.at("data").is_object());
    CHECK(success.at("warnings").is_array());

    const lx::Error error{lx::ErrorCode::NotFound, "missing", 2, "test", "get"};
    const auto failure = nlohmann::json::parse(
        lx::cli::JsonSerializer::error("process", "inspect", error));
    CHECK(failure.at("error").at("code") == "not_found");
    CHECK(failure.at("error").at("system_error") == 2);
}
