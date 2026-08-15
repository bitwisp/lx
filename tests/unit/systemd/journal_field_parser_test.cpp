#include "lx/linux/systemd/JournalFieldParser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("journal field parser respects explicit data length")
{
    const char data[] = {'M', 'E', 'S', 'S', 'A', 'G', 'E', '=', 'a', '\0', 'b'};
    const auto value = lx::linux::journalFieldValue("MESSAGE", data,
                                                     sizeof(data));

    REQUIRE(value);
    REQUIRE(value.value().size() == 3);
    REQUIRE(value.value()[0] == 'a');
    REQUIRE(value.value()[1] == '\0');
    REQUIRE(value.value()[2] == 'b');
}

TEST_CASE("journal field parser rejects a mismatched field")
{
    constexpr char data[] = "_COMM=demo";
    const auto value = lx::linux::journalFieldValue(
        "MESSAGE", data, sizeof(data) - 1);

    REQUIRE_FALSE(value);
    REQUIRE(value.error().code == lx::ErrorCode::ProtocolError);
}

TEST_CASE("journal entry parser converts typed metadata")
{
    lx::linux::JournalFields fields;
    fields.timestampUsec = 42000000;
    fields.cursor = "cursor";
    fields.systemdUnit = "demo.service";
    fields.pid = "123";
    fields.command = "demo";
    fields.message = "started";
    fields.priority = "6";

    const auto observed = lx::linux::journalEntryFromFields(std::move(fields));
    REQUIRE(observed.value.pid == 123);
    REQUIRE(observed.value.priority == 6);
    REQUIRE(observed.value.message == "started");
    REQUIRE(observed.warnings.empty());
}

TEST_CASE("journal entry parser warns about invalid optional metadata")
{
    lx::linux::JournalFields fields;
    fields.pid = "12x";
    fields.priority = "9";

    const auto observed = lx::linux::journalEntryFromFields(std::move(fields));
    REQUIRE_FALSE(observed.value.pid);
    REQUIRE_FALSE(observed.value.priority);
    REQUIRE(observed.warnings.size() == 2);
}
