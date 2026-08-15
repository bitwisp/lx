#pragma once

#include "lx/contracts/IJournalProvider.h"

namespace lx::linux {

class SystemdJournalProvider final : public contracts::IJournalProvider {
public:
    Result<void> probe() const override;
    Result<Observation<std::vector<JournalEntry>>> query(
        const LogQuery& query) const override;
    Result<void> follow(
        const LogQuery& query, const contracts::JournalEntrySink& sink,
        const contracts::JournalStopRequested& stopRequested) const override;
};

} // namespace lx::linux
