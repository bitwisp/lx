#pragma once

#include "lx/domain/JournalEntry.h"
#include "lx/domain/LogQuery.h"
#include "lx/domain/Observation.h"
#include "lx/domain/Result.h"

#include <functional>
#include <vector>

namespace lx::contracts {

using JournalEntrySink =
    std::function<Result<void>(const Observation<JournalEntry>&)>;
using JournalStopRequested = std::function<bool()>;

class IJournalProvider {
public:
    virtual ~IJournalProvider() = default;

    virtual Result<void> probe() const = 0;
    virtual Result<Observation<std::vector<JournalEntry>>> query(
        const LogQuery& query) const = 0;
    virtual Result<void> follow(
        const LogQuery& query, const JournalEntrySink& sink,
        const JournalStopRequested& stopRequested) const = 0;
};

} // namespace lx::contracts
