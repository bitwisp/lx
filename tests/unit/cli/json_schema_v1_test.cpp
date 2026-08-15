#include "lx/cli/JsonSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <set>
#include <string>

namespace {
std::set<std::string> keys(const nlohmann::json& value)
{
    std::set<std::string> result;
    for (auto item = value.begin(); item != value.end(); ++item) {
        result.insert(item.key());
    }
    return result;
}
} // namespace

TEST_CASE("JSON schema v1 locks common envelope keys")
{
    const auto success = nlohmann::json::parse(
        lx::cli::JsonSerializer::emptySuccess("doctor", "inspect"));
    CHECK(keys(success) == std::set<std::string>{
                               "command", "data", "operation",
                               "schema_version", "warnings"});

    const lx::Error error{lx::ErrorCode::Conflict, "ambiguous", 0,
                          "resolver", "resolve"};
    const auto failure = nlohmann::json::parse(
        lx::cli::JsonSerializer::error("inspect", "inspect", error));
    CHECK(keys(failure) == std::set<std::string>{
                               "command", "error", "operation",
                               "schema_version"});
    CHECK(keys(failure.at("error")) == std::set<std::string>{
                                                "code", "component", "message",
                                                "operation", "system_error"});
}

TEST_CASE("JSON schema v1 locks process field names and primitive types")
{
    lx::ProcessInfo process;
    process.pid = 7;
    const auto document = nlohmann::json::parse(
        lx::cli::JsonSerializer::process({process, {}}));
    const auto& value = document.at("data").at("process");
    CHECK(keys(value) == std::set<std::string>{
                             "argv", "cwd", "executable", "gid", "name",
                             "pid", "ppid", "rss_bytes", "state",
                             "systemd_unit", "threads", "uid", "user"});
    CHECK(value.at("pid").is_number_integer());
    CHECK(value.at("argv").is_array());
    CHECK(value.at("executable").is_null());
}
