#include "lx/domain/ProcessQuery.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("process query has no filters by default")
{
    const lx::ProcessQuery query;
    CHECK_FALSE(query.name);
    CHECK_FALSE(query.user);
    CHECK_FALSE(query.systemdUnit);
}
