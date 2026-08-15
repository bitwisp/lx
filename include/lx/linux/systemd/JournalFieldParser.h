#pragma once

#include "lx/domain/JournalEntry.h"
#include "lx/domain/Observation.h"
#include "lx/domain/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace lx::linux {

struct JournalFields {
    std::uint64_t timestampUsec = 0;
    std::string cursor;
    std::optional<std::string> systemdUnit;
    std::optional<std::string> pid;
    std::optional<std::string> command;
    std::optional<std::string> message;
    std::optional<std::string> priority;
};

[[nodiscard]] Result<std::string> journalFieldValue(
    std::string_view field, const void* data, std::size_t length);
[[nodiscard]] Observation<JournalEntry> journalEntryFromFields(
    JournalFields fields);

} // namespace lx::linux
