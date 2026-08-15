#include "lx/linux/procfs/StatParser.h"

#include <charconv>
#include <limits>
#include <vector>
#include <string>

namespace lx::linux::procfs {
namespace {

Result<StatRecord> parseFailure(const std::string& message)
{
    return Result<StatRecord>::failure(
        {ErrorCode::ParseError, message, 0, "procfs-stat", "parse"});
}

bool parsePid(const std::string_view text, pid_t& value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
           value >= 0;
}

bool parseCounter(const std::string_view text, std::uint64_t& value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

} // namespace

Result<StatRecord> parseStat(const std::string_view contents)
{
    const auto open = contents.find('(');
    const auto close = contents.rfind(") ");
    if (open == std::string_view::npos || close == std::string_view::npos ||
        open == 0 || close <= open || close + 4 > contents.size()) {
        return parseFailure("Malformed process stat record");
    }

    auto pidText = contents.substr(0, open);
    while (!pidText.empty() && pidText.back() == ' ') {
        pidText.remove_suffix(1);
    }

    StatRecord record;
    if (!parsePid(pidText, record.pid)) {
        return parseFailure("Invalid PID in process stat record");
    }
    record.name = std::string(contents.substr(open + 1, close - open - 1));
    record.state = contents[close + 2];

    const auto ppidBegin = close + 4;
    auto ppidEnd = contents.find(' ', ppidBegin);
    if (ppidEnd == std::string_view::npos) {
        ppidEnd = contents.find('\n', ppidBegin);
    }
    if (ppidEnd == std::string_view::npos) {
        ppidEnd = contents.size();
    }
    if (!parsePid(contents.substr(ppidBegin, ppidEnd - ppidBegin), record.ppid)) {
        return parseFailure("Invalid PPID in process stat record");
    }
    return Result<StatRecord>::success(std::move(record));
}

Result<ProcessCpuSample> parseProcessCpuStat(const std::string_view contents)
{
    const auto basic = parseStat(contents);
    if (!basic) {
        return Result<ProcessCpuSample>::failure(basic.error());
    }
    const auto close = contents.rfind(") ");
    auto tail = contents.substr(close + 4); // field 4 (ppid)
    std::vector<std::string_view> fields;
    while (!tail.empty()) {
        const auto begin = tail.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos) break;
        tail.remove_prefix(begin);
        const auto end = tail.find_first_of(" \t\r\n");
        fields.push_back(tail.substr(0, end));
        if (end == std::string_view::npos) break;
        tail.remove_prefix(end);
    }
    if (fields.size() <= 18) {
        return Result<ProcessCpuSample>::failure(
            {ErrorCode::ParseError, "Incomplete process CPU stat record", 0,
             "procfs-stat", "parse"});
    }
    std::uint64_t user = 0;
    std::uint64_t system = 0;
    std::uint64_t start = 0;
    if (!parseCounter(fields[10], user) || !parseCounter(fields[11], system) ||
        !parseCounter(fields[18], start) ||
        system > std::numeric_limits<std::uint64_t>::max() - user) {
        return Result<ProcessCpuSample>::failure(
            {ErrorCode::ParseError, "Invalid process CPU stat counters", 0,
             "procfs-stat", "parse"});
    }
    return Result<ProcessCpuSample>::success(
        {basic.value().pid, start, user + system});
}

} // namespace lx::linux::procfs
