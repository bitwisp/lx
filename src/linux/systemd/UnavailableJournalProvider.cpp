#include "lx/linux/systemd/UnavailableJournalProvider.h"

#include <utility>

namespace lx::linux {

UnavailableJournalProvider::UnavailableJournalProvider(std::string reason)
    : reason_(std::move(reason))
{
}

Error UnavailableJournalProvider::unavailable(
    const std::string& operation) const
{
    return {ErrorCode::Unavailable, reason_, 0, "journal", operation};
}

Result<void> UnavailableJournalProvider::probe() const
{
    return Result<void>::failure(unavailable("probe"));
}

Result<Observation<std::vector<JournalEntry>>>
UnavailableJournalProvider::query(const LogQuery&) const
{
    return Result<Observation<std::vector<JournalEntry>>>::failure(
        unavailable("query"));
}

Result<void> UnavailableJournalProvider::follow(
    const LogQuery&, const contracts::JournalEntrySink&,
    const contracts::JournalStopRequested&) const
{
    return Result<void>::failure(unavailable("follow"));
}

} // namespace lx::linux
