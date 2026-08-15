#include "lx/application/LogService.h"

#include "lx/application/ServiceService.h"

#include <charconv>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace lx::application {
namespace {

template <typename T>
Result<T> invalid(const std::string& message, const std::string& operation)
{
    return Result<T>::failure(
        {ErrorCode::InvalidArgument, message, 0, "log", operation});
}

bool sameLocalTime(const std::tm& expected, const std::time_t value)
{
    std::tm actual{};
    if (localtime_r(&value, &actual) == nullptr) return false;
    return actual.tm_year == expected.tm_year &&
           actual.tm_mon == expected.tm_mon &&
           actual.tm_mday == expected.tm_mday &&
           actual.tm_hour == expected.tm_hour &&
           actual.tm_min == expected.tm_min && actual.tm_sec == expected.tm_sec;
}

Result<std::chrono::system_clock::time_point> parseRelative(
    const std::string& value,
    const std::chrono::system_clock::time_point now)
{
    if (value.size() < 2) {
        return invalid<std::chrono::system_clock::time_point>(
            "Invalid relative time; use forms such as 30s, 10m, 2h, or 3d",
            "parse since");
    }
    std::uint64_t amount = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size() - 1,
                                        amount);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size() - 1) {
        return invalid<std::chrono::system_clock::time_point>(
            "Invalid relative time; use forms such as 30s, 10m, 2h, or 3d",
            "parse since");
    }

    std::uint64_t multiplier = 0;
    switch (value.back()) {
    case 's': multiplier = 1; break;
    case 'm': multiplier = 60; break;
    case 'h': multiplier = 60 * 60; break;
    case 'd': multiplier = 24 * 60 * 60; break;
    default:
        return invalid<std::chrono::system_clock::time_point>(
            "Invalid relative time unit; expected s, m, h, or d",
            "parse since");
    }
    if (amount > static_cast<std::uint64_t>(
                     std::numeric_limits<std::int64_t>::max()) /
                     multiplier) {
        return invalid<std::chrono::system_clock::time_point>(
            "Relative time is out of range", "parse since");
    }
    const auto seconds = std::chrono::seconds{
        static_cast<std::int64_t>(amount * multiplier)};
    const auto since = now - seconds;
    if (since.time_since_epoch() <
        std::chrono::system_clock::duration::zero()) {
        return invalid<std::chrono::system_clock::time_point>(
            "Relative time precedes the Unix epoch", "parse since");
    }
    return Result<std::chrono::system_clock::time_point>::success(since);
}

} // namespace

LogService::LogService(
    const contracts::IJournalProvider& journalProvider) noexcept
    : journalProvider_(journalProvider)
{
}

Result<LogQuery> LogService::normalize(LogQuery query)
{
    if (!query.unit && !query.pid) {
        return invalid<LogQuery>(
            "A service unit or PID is required for log queries",
            "validate query");
    }
    if (query.pid && *query.pid <= 0) {
        return invalid<LogQuery>("PID must be greater than zero",
                                 "validate query");
    }
    if (query.limit == 0 || query.limit > 10000) {
        return invalid<LogQuery>("Log line limit must be between 1 and 10000",
                                 "validate query");
    }
    if (query.unit) {
        auto unit = ServiceService::normalizeUnit(*query.unit);
        if (!unit) return Result<LogQuery>::failure(unit.error());
        query.unit = std::move(unit).value();
    }
    return Result<LogQuery>::success(std::move(query));
}

Result<Observation<std::vector<JournalEntry>>> LogService::read(
    LogQuery query) const
{
    auto normalized = normalize(std::move(query));
    if (!normalized) {
        return Result<Observation<std::vector<JournalEntry>>>::failure(
            normalized.error());
    }
    return journalProvider_.query(normalized.value());
}

Result<void> LogService::follow(
    LogQuery query, const contracts::JournalEntrySink& sink,
    const contracts::JournalStopRequested& stopRequested) const
{
    auto normalized = normalize(std::move(query));
    if (!normalized) return Result<void>::failure(normalized.error());
    normalized.value().follow = true;
    return journalProvider_.follow(normalized.value(), sink, stopRequested);
}

Result<std::chrono::system_clock::time_point> LogService::parseSince(
    const std::string& value,
    const std::chrono::system_clock::time_point now)
{
    if (!value.empty()) {
        const char suffix = value.back();
        if (suffix == 's' || suffix == 'm' || suffix == 'h' || suffix == 'd') {
            return parseRelative(value, now);
        }
    }

    std::tm parsedTime{};
    parsedTime.tm_isdst = -1;
    std::istringstream stream{value};
    stream >> std::get_time(&parsedTime, "%Y-%m-%d %H:%M:%S");
    if (stream.fail()) {
        return invalid<std::chrono::system_clock::time_point>(
            "Invalid absolute time; expected YYYY-MM-DD HH:MM:SS",
            "parse since");
    }
    stream >> std::ws;
    if (!stream.eof()) {
        return invalid<std::chrono::system_clock::time_point>(
            "Absolute time contains trailing characters", "parse since");
    }
    const std::tm expected = parsedTime;
    const std::time_t converted = std::mktime(&parsedTime);
    if (converted < 0 || !sameLocalTime(expected, converted)) {
        return invalid<std::chrono::system_clock::time_point>(
            "Absolute time is not a valid local date", "parse since");
    }
    return Result<std::chrono::system_clock::time_point>::success(
        std::chrono::system_clock::from_time_t(converted));
}

} // namespace lx::application
