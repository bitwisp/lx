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

TEST_CASE("JSON serializer emits complete redacted process resources")
{
    lx::ProcessInfo process;
    process.pid = 42;
    process.ppid = 1;
    process.name = "demo";
    process.argv = {"demo", "--token", "secret", "--port=80"};
    const lx::Observation<lx::ProcessInfo> observed{process, {}};

    const auto value = nlohmann::json::parse(
        lx::cli::JsonSerializer::process(observed));
    const auto& data = value.at("data").at("process");
    CHECK(data.at("pid") == 42);
    CHECK(data.at("executable").is_null());
    CHECK(data.at("systemd_unit").is_null());
    CHECK(data.at("argv").at(2) == "<redacted>");

    const auto raw = nlohmann::json::parse(
        lx::cli::JsonSerializer::process(observed, true));
    CHECK(raw.at("data").at("process").at("argv").at(2) == "secret");
}
