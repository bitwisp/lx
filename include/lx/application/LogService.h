#pragma once

#include "lx/contracts/IJournalProvider.h"

#include <chrono>
#include <string>

namespace lx::application {

class LogService final {
public:
    explicit LogService(
        const contracts::IJournalProvider& journalProvider) noexcept;

    [[nodiscard]] Result<Observation<std::vector<JournalEntry>>> read(
        LogQuery query) const;
    [[nodiscard]] Result<void> follow(
        LogQuery query, const contracts::JournalEntrySink& sink,
        const contracts::JournalStopRequested& stopRequested) const;

    [[nodiscard]] static Result<std::chrono::system_clock::time_point>
    parseSince(const std::string& value,
               std::chrono::system_clock::time_point now =
                   std::chrono::system_clock::now());

private:
    [[nodiscard]] static Result<LogQuery> normalize(LogQuery query);

    const contracts::IJournalProvider& journalProvider_;
};

} // namespace lx::application
