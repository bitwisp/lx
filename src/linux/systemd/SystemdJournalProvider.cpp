#include "lx/linux/systemd/SystemdJournalProvider.h"

#include "lx/linux/systemd/JournalFieldParser.h"
#include "lx/linux/systemd/SdJournal.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace lx::linux {
namespace {

constexpr std::size_t journalDataThreshold = 1024U * 1024U;

Error journalError(const int status, const std::string& operation)
{
    const int number = status < 0 ? -status : status;
    ErrorCode code = ErrorCode::IoError;
    if (number == EACCES || number == EPERM) code = ErrorCode::PermissionDenied;
    else if (number == ENOENT) code = ErrorCode::Unavailable;
    else if (number == EINTR) code = ErrorCode::Interrupted;
    else if (number == ETIMEDOUT) code = ErrorCode::Timeout;
    return {code,
            "Journal operation failed: " +
                std::system_category().message(number),
            number, "sd-journal", operation};
}

Result<void> configureJournal(sd_journal* journal, const LogQuery& query)
{
    if (query.limit == 0) {
        return Result<void>::failure(
            {ErrorCode::InvalidArgument, "Journal limit must be greater than zero",
             0, "sd-journal", "configure query"});
    }
    const int thresholdStatus =
        sd_journal_set_data_threshold(journal, journalDataThreshold);
    if (thresholdStatus < 0) {
        return Result<void>::failure(
            journalError(thresholdStatus, "set data threshold"));
    }

    const auto addMatch = [journal](const std::string& match) {
        const int status = sd_journal_add_match(journal, match.data(), 0);
        return status < 0
                   ? Result<void>::failure(journalError(status, "add match"))
                   : Result<void>::success();
    };
    if (query.unit) {
        auto matched = addMatch("_SYSTEMD_UNIT=" + *query.unit);
        if (!matched) return matched;
    }
    if (query.pid) {
        auto matched = addMatch("_PID=" + std::to_string(*query.pid));
        if (!matched) return matched;
    }
    return Result<void>::success();
}

Result<std::optional<std::string>> readField(sd_journal* journal,
                                             const std::string& field)
{
    const void* data = nullptr;
    std::size_t length = 0;
    const int status =
        sd_journal_get_data(journal, field.c_str(), &data, &length);
    if (status == -ENOENT) {
        return Result<std::optional<std::string>>::success(std::nullopt);
    }
    if (status < 0) {
        return Result<std::optional<std::string>>::failure(
            journalError(status, "read field"));
    }
    auto value = journalFieldValue(field, data, length);
    if (!value) {
        return Result<std::optional<std::string>>::failure(value.error());
    }
    return Result<std::optional<std::string>>::success(
        std::optional<std::string>{std::move(value).value()});
}

Result<Observation<JournalEntry>> readCurrentEntry(sd_journal* journal)
{
    std::uint64_t timestampUsec = 0;
    const int timestampStatus =
        sd_journal_get_realtime_usec(journal, &timestampUsec);
    if (timestampStatus < 0) {
        return Result<Observation<JournalEntry>>::failure(
            journalError(timestampStatus, "read timestamp"));
    }
    if (timestampUsec >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return Result<Observation<JournalEntry>>::failure(
            {ErrorCode::ParseError, "Journal timestamp is out of range", 0,
             "sd-journal", "read timestamp"});
    }

    JournalCursor cursor;
    const int cursorStatus = sd_journal_get_cursor(journal, cursor.put());
    if (cursorStatus < 0) {
        return Result<Observation<JournalEntry>>::failure(
            journalError(cursorStatus, "read cursor"));
    }

    JournalFields fields;
    fields.timestampUsec = timestampUsec;
    fields.cursor = cursor.get() == nullptr ? "" : cursor.get();

    auto unit = readField(journal, "_SYSTEMD_UNIT");
    auto pid = readField(journal, "_PID");
    auto command = readField(journal, "_COMM");
    auto message = readField(journal, "MESSAGE");
    auto priority = readField(journal, "PRIORITY");
    if (!unit) return Result<Observation<JournalEntry>>::failure(unit.error());
    if (!pid) return Result<Observation<JournalEntry>>::failure(pid.error());
    if (!command) return Result<Observation<JournalEntry>>::failure(command.error());
    if (!message) return Result<Observation<JournalEntry>>::failure(message.error());
    if (!priority) return Result<Observation<JournalEntry>>::failure(priority.error());
    fields.systemdUnit = std::move(unit).value();
    fields.pid = std::move(pid).value();
    fields.command = std::move(command).value();
    fields.message = std::move(message).value();
    fields.priority = std::move(priority).value();

    auto observed = journalEntryFromFields(std::move(fields));
    if (observed.value.message.empty()) {
        observed.warnings.push_back(
            {ErrorCode::NotFound, "Journal entry has no MESSAGE field", 0,
             "sd-journal", "read entry"});
    }
    return Result<Observation<JournalEntry>>::success(std::move(observed));
}

void appendEntry(Observation<std::vector<JournalEntry>>& target,
                 Observation<JournalEntry> source)
{
    target.value.push_back(std::move(source.value));
    target.warnings.insert(
        target.warnings.end(),
        std::make_move_iterator(source.warnings.begin()),
        std::make_move_iterator(source.warnings.end()));
}

Result<Observation<std::vector<JournalEntry>>> queryTail(
    sd_journal* journal, const std::size_t limit)
{
    const int seekStatus = sd_journal_seek_tail(journal);
    if (seekStatus < 0) {
        return Result<Observation<std::vector<JournalEntry>>>::failure(
            journalError(seekStatus, "seek tail"));
    }

    std::vector<Observation<JournalEntry>> reversed;
    reversed.reserve(limit);
    while (reversed.size() < limit) {
        const int status = sd_journal_previous(journal);
        if (status < 0) {
            return Result<Observation<std::vector<JournalEntry>>>::failure(
                journalError(status, "read previous entry"));
        }
        if (status == 0) break;
        auto entry = readCurrentEntry(journal);
        if (!entry) {
            return Result<Observation<std::vector<JournalEntry>>>::failure(
                entry.error());
        }
        reversed.push_back(std::move(entry).value());
    }

    Observation<std::vector<JournalEntry>> result;
    result.value.reserve(reversed.size());
    for (auto iterator = reversed.rbegin(); iterator != reversed.rend();
         ++iterator) {
        appendEntry(result, std::move(*iterator));
    }
    return Result<Observation<std::vector<JournalEntry>>>::success(
        std::move(result));
}

Result<Observation<std::vector<JournalEntry>>> querySince(
    sd_journal* journal,
    const std::chrono::system_clock::time_point since,
    const std::size_t limit)
{
    const auto signedUsec = std::chrono::duration_cast<std::chrono::microseconds>(
                                since.time_since_epoch())
                                .count();
    if (signedUsec < 0) {
        return Result<Observation<std::vector<JournalEntry>>>::failure(
            {ErrorCode::InvalidArgument,
             "Journal since time must not precede the Unix epoch", 0,
             "sd-journal", "seek time"});
    }
    const int seekStatus = sd_journal_seek_realtime_usec(
        journal, static_cast<std::uint64_t>(signedUsec));
    if (seekStatus < 0) {
        return Result<Observation<std::vector<JournalEntry>>>::failure(
            journalError(seekStatus, "seek time"));
    }

    std::deque<Observation<JournalEntry>> window;
    while (true) {
        const int status = sd_journal_next(journal);
        if (status < 0) {
            return Result<Observation<std::vector<JournalEntry>>>::failure(
                journalError(status, "read next entry"));
        }
        if (status == 0) break;
        auto entry = readCurrentEntry(journal);
        if (!entry) {
            return Result<Observation<std::vector<JournalEntry>>>::failure(
                entry.error());
        }
        window.push_back(std::move(entry).value());
        if (window.size() > limit) window.pop_front();
    }

    Observation<std::vector<JournalEntry>> result;
    result.value.reserve(window.size());
    while (!window.empty()) {
        appendEntry(result, std::move(window.front()));
        window.pop_front();
    }
    return Result<Observation<std::vector<JournalEntry>>>::success(
        std::move(result));
}

} // namespace

Result<void> SystemdJournalProvider::probe() const
{
    auto journal = SdJournal::openLocal();
    if (!journal) return Result<void>::failure(journal.error());
    return Result<void>::success();
}

Result<Observation<std::vector<JournalEntry>>>
SystemdJournalProvider::query(const LogQuery& query) const
{
    auto journal = SdJournal::openLocal();
    if (!journal) {
        return Result<Observation<std::vector<JournalEntry>>>::failure(
            journal.error());
    }
    auto configured = configureJournal(journal.value().get(), query);
    if (!configured) {
        return Result<Observation<std::vector<JournalEntry>>>::failure(
            configured.error());
    }
    return query.since ? querySince(journal.value().get(), *query.since,
                                    query.limit)
                       : queryTail(journal.value().get(), query.limit);
}

Result<void> SystemdJournalProvider::follow(
    const LogQuery&, const contracts::JournalEntrySink&,
    const contracts::JournalStopRequested&) const
{
    return Result<void>::failure(
        {ErrorCode::Unsupported, "Journal follow is not implemented yet", 0,
         "sd-journal", "follow"});
}

} // namespace lx::linux
