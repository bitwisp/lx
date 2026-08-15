#pragma once

#include "lx/contracts/IJournalProvider.h"

#include <string>

namespace lx::linux {

class UnavailableJournalProvider final
    : public contracts::IJournalProvider {
public:
    explicit UnavailableJournalProvider(
        std::string reason = "journal support is unavailable");

    Result<void> probe() const override;
    Result<Observation<std::vector<JournalEntry>>> query(
        const LogQuery& query) const override;
    Result<void> follow(
        const LogQuery& query, const contracts::JournalEntrySink& sink,
        const contracts::JournalStopRequested& stopRequested) const override;

private:
    [[nodiscard]] Error unavailable(const std::string& operation) const;

    std::string reason_;
};

} // namespace lx::linux
