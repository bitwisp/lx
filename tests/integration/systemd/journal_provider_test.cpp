#include "lx/linux/systemd/SystemdJournalProvider.h"

#include <catch2/catch_test_macros.hpp>
#include <unistd.h>

TEST_CASE("journal provider performs read-only local queries")
{
    const lx::linux::SystemdJournalProvider provider;
    const auto probe = provider.probe();
    if (!probe) {
        SKIP("local journal unavailable: " + probe.error().message);
    }

    lx::LogQuery query;
    query.pid = ::getpid();
    query.limit = 10;
    query.since = std::chrono::system_clock::now() - std::chrono::minutes{1};
    const auto entries = provider.query(query);

    REQUIRE(entries);
    REQUIRE(entries.value().value.size() <= query.limit);
    for (const auto& entry : entries.value().value) {
        REQUIRE(entry.pid == ::getpid());
    }
}

TEST_CASE("journal follow honors an immediate stop request")
{
    const lx::linux::SystemdJournalProvider provider;
    const auto probe = provider.probe();
    if (!probe) {
        SKIP("local journal unavailable: " + probe.error().message);
    }

    lx::LogQuery query;
    query.pid = ::getpid();
    query.limit = 1;
    const auto result = provider.follow(
        query,
        [](const lx::Observation<lx::JournalEntry>&) {
            return lx::Result<void>::success();
        },
        [] { return true; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::Interrupted);
}
