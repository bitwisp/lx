#include "lx/linux/systemd/UnavailableJournalProvider.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("unavailable journal provider returns structured failures")
{
    const lx::linux::UnavailableJournalProvider provider{"journal missing"};
    const lx::LogQuery query;

    const auto probe = provider.probe();
    const auto entries = provider.query(query);
    const auto followed = provider.follow(
        query,
        [](const lx::Observation<lx::JournalEntry>&) {
            return lx::Result<void>::success();
        },
        [] { return true; });

    REQUIRE_FALSE(probe);
    REQUIRE(probe.error().code == lx::ErrorCode::Unavailable);
    REQUIRE(probe.error().message == "journal missing");
    REQUIRE_FALSE(entries);
    REQUIRE_FALSE(followed);
}
