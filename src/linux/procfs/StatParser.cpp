#include "lx/linux/procfs/StatParser.h"

#include <charconv>
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

} // namespace lx::linux::procfs

