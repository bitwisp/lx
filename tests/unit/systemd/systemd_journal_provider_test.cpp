#include "lx/linux/systemd/SystemdJournalProvider.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("systemd journal provider rejects a zero query limit")
{
    const lx::linux::SystemdJournalProvider provider;
    lx::LogQuery query;
    query.limit = 0;

    const auto result = provider.query(query);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::InvalidArgument);
}
