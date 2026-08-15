#include "lx/domain/JournalEntry.h"
#include "lx/domain/LogQuery.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("journal entry preserves structured metadata")
{
    lx::JournalEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point{
        std::chrono::seconds{42}};
    entry.cursor = "cursor";
    entry.systemdUnit = "demo.service";
    entry.pid = 123;
    entry.command = "demo";
    entry.message = "started";
    entry.priority = 6;

    REQUIRE(entry.systemdUnit == "demo.service");
    REQUIRE(entry.pid == 123);
    REQUIRE(entry.message == "started");
    REQUIRE(entry.priority == 6);
}

TEST_CASE("log query has a conservative default limit")
{
    const lx::LogQuery query;
    REQUIRE(query.limit == 50);
    REQUIRE_FALSE(query.follow);
    REQUIRE_FALSE(query.unit);
    REQUIRE_FALSE(query.pid);
}
