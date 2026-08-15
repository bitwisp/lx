#include "lx/application/LogService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class FakeJournalProvider final : public lx::contracts::IJournalProvider {
public:
    lx::Result<void> probe() const override { return lx::Result<void>::success(); }

    lx::Result<lx::Observation<std::vector<lx::JournalEntry>>> query(
        const lx::LogQuery& query) const override
    {
        lastQuery = query;
        return lx::Result<lx::Observation<std::vector<lx::JournalEntry>>>::success(
            {{}, {}});
    }

    lx::Result<void> follow(
        const lx::LogQuery& query, const lx::contracts::JournalEntrySink& sink,
        const lx::contracts::JournalStopRequested&) const override
    {
        lastQuery = query;
        lx::JournalEntry entry;
        entry.message = "followed";
        return sink({std::move(entry), {}});
    }

    mutable lx::LogQuery lastQuery;
};

} // namespace

TEST_CASE("log service validates and normalizes filters")
{
    FakeJournalProvider provider;
    const lx::application::LogService service{provider};

    lx::LogQuery query;
    query.unit = "demo";
    query.pid = 42;
    query.limit = 100;
    const auto result = service.read(query);

    REQUIRE(result);
    REQUIRE(provider.lastQuery.unit == "demo.service");
    REQUIRE(provider.lastQuery.pid == 42);
    REQUIRE(provider.lastQuery.limit == 100);

    REQUIRE_FALSE(service.read({}));
    query.limit = 10001;
    REQUIRE_FALSE(service.read(query));
}

TEST_CASE("log service parses compact relative times")
{
    const auto now = std::chrono::system_clock::time_point{
        std::chrono::seconds{100000}};
    const auto since = lx::application::LogService::parseSince("10m", now);
    REQUIRE(since);
    REQUIRE(since.value() == now - std::chrono::minutes{10});
    REQUIRE_FALSE(lx::application::LogService::parseSince("10weeks", now));
    REQUIRE_FALSE(lx::application::LogService::parseSince(
        "999999999999999999999d", now));
}

TEST_CASE("log service parses and validates absolute local times")
{
    const auto valid = lx::application::LogService::parseSince(
        "2026-08-15 10:30:00");
    REQUIRE(valid);
    REQUIRE_FALSE(lx::application::LogService::parseSince(
        "2026-02-31 10:30:00"));
    REQUIRE_FALSE(lx::application::LogService::parseSince(
        "2026-08-15 10:30:00 trailing"));
}

TEST_CASE("log service forwards follow entries")
{
    FakeJournalProvider provider;
    const lx::application::LogService service{provider};
    lx::LogQuery query;
    query.pid = 42;
    std::string message;

    const auto result = service.follow(
        query,
        [&message](const lx::Observation<lx::JournalEntry>& entry) {
            message = entry.value.message;
            return lx::Result<void>::success();
        },
        [] { return false; });

    REQUIRE(result);
    REQUIRE(provider.lastQuery.follow);
    REQUIRE(message == "followed");
}
