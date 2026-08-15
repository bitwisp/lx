#include "lx/linux/systemd/SdJournal.h"

#include <catch2/catch_test_macros.hpp>
#include <utility>

TEST_CASE("sd-journal wrappers safely own native resources")
{
    lx::linux::JournalCursor cursor;
    lx::linux::JournalCursor moved{std::move(cursor)};
    REQUIRE(moved.get() == nullptr);
}

TEST_CASE("sd-journal local open succeeds or returns a classified failure")
{
    auto journal = lx::linux::SdJournal::openLocal();
    if (journal) {
        REQUIRE(journal.value().get() != nullptr);
    } else {
        REQUIRE((journal.error().code == lx::ErrorCode::Unavailable ||
                 journal.error().code == lx::ErrorCode::PermissionDenied));
    }
}
