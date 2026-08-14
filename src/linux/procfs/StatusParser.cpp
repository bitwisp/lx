#include "lx/linux/procfs/StatusParser.h"

#include <charconv>
#include <limits>
#include <string>

namespace lx::linux::procfs {
namespace {

Result<StatusRecord> failure(const std::string& message)
{
    return Result<StatusRecord>::failure(
        {ErrorCode::ParseError, message, 0, "procfs-status", "parse"});
}

std::string_view firstToken(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t");
    if (begin == std::string_view::npos) return {};
    value.remove_prefix(begin);
    const auto end = value.find_first_of(" \t\r");
    return value.substr(0, end);
}

template <typename T>
bool parseUnsigned(const std::string_view text, T& value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && parsed.ec == std::errc{} &&
           parsed.ptr == text.data() + text.size();
}

} // namespace

Result<StatusRecord> parseStatus(const std::string_view contents)
{
    StatusRecord record;
    bool hasUid = false;
    bool hasGid = false;
    bool hasThreads = false;
    std::size_t position = 0;
    while (position < contents.size()) {
        auto end = contents.find('\n', position);
        if (end == std::string_view::npos) end = contents.size();
        const auto line = contents.substr(position, end - position);
        const auto colon = line.find(':');
        if (colon != std::string_view::npos) {
            const auto key = line.substr(0, colon);
            const auto token = firstToken(line.substr(colon + 1));
            if (key == "Uid") {
                hasUid = parseUnsigned(token, record.uid);
                if (!hasUid) return failure("Invalid Uid field");
            } else if (key == "Gid") {
                hasGid = parseUnsigned(token, record.gid);
                if (!hasGid) return failure("Invalid Gid field");
            } else if (key == "Threads") {
                hasThreads = parseUnsigned(token, record.threads);
                if (!hasThreads) return failure("Invalid Threads field");
            } else if (key == "VmRSS") {
                std::uint64_t kib = 0;
                if (!parseUnsigned(token, kib) ||
                    kib > std::numeric_limits<std::uint64_t>::max() / 1024) {
                    return failure("Invalid VmRSS field");
                }
                record.rssBytes = kib * 1024;
            }
        }
        position = end + 1;
    }
    if (!hasUid || !hasGid || !hasThreads) {
        return failure("Required process status field is missing");
    }
    return Result<StatusRecord>::success(record);
}

} // namespace lx::linux::procfs

