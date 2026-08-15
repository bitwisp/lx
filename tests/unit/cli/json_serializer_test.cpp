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

TEST_CASE("JSON serializer emits nested port owners and service state")
{
    lx::PortInfo port;
    port.socket.protocol = lx::TransportProtocol::Tcp;
    port.socket.family = lx::AddressFamily::IPv6;
    port.socket.local = {"::", 8080};
    port.socket.ownerPids = {42};
    lx::ProcessInfo owner;
    owner.pid = 42;
    port.owners.push_back(owner);
    const auto ports = nlohmann::json::parse(
        lx::cli::JsonSerializer::ports({{{port}}, {}}));
    const auto& socket = ports.at("data").at("ports").at(0).at("socket");
    CHECK(socket.at("protocol") == "tcp");
    CHECK(socket.at("family") == "ipv6");
    CHECK(socket.at("remote").is_null());
    CHECK(ports.at("data").at("ports").at(0).at("owners").at(0).at("pid") == 42);

    lx::ServiceInfo service;
    service.unitName = "demo.service";
    service.activeState = "active";
    const auto serialized = nlohmann::json::parse(
        lx::cli::JsonSerializer::service({service, {}}));
    CHECK(serialized.at("data").at("service").at("main_pid").is_null());
    CHECK(serialized.at("data").at("service").at("active_state") == "active");
}

TEST_CASE("JSON serializer emits finite logs and NDJSON event documents")
{
    lx::JournalEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point{
        std::chrono::microseconds{1234567}};
    entry.pid = 42;
    entry.message = "first\nsecond\x1b";
    entry.priority = 6;
    const auto finite = nlohmann::json::parse(
        lx::cli::JsonSerializer::logs({{{entry}}, {}}));
    const auto& value = finite.at("data").at("entries").at(0);
    CHECK(value.at("timestamp_unix_usec") == 1234567);
    CHECK(value.at("message") == entry.message);
    CHECK(value.at("systemd_unit").is_null());

    const auto event = nlohmann::json::parse(
        lx::cli::JsonSerializer::logEvent({entry, {}}));
    CHECK(event.at("operation") == "follow");
    CHECK(event.at("data").at("entry").at("pid") == 42);
}
