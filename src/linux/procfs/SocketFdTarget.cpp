#include "lx/linux/procfs/SocketFdTarget.h"

#include <charconv>
#include <system_error>

namespace lx::linux::procfs {

Result<std::optional<std::uint64_t>> parseSocketFdTarget(
    const std::string_view target)
{
    constexpr std::string_view prefix = "socket:[";
    if (target.compare(0, prefix.size(), prefix) != 0) {
        return Result<std::optional<std::uint64_t>>::success(std::nullopt);
    }
    if (target.size() <= prefix.size() + 1 || target.back() != ']') {
        return Result<std::optional<std::uint64_t>>::failure({
            ErrorCode::ParseError, "Malformed socket file descriptor target", 0,
            "socket-fd-target", "parse"});
    }

    const auto digits = target.substr(
        prefix.size(), target.size() - prefix.size() - 1);
    std::uint64_t inode = 0;
    const auto parsed = std::from_chars(
        digits.data(), digits.data() + digits.size(), inode);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
        return Result<std::optional<std::uint64_t>>::failure({
            ErrorCode::ParseError, "Invalid socket inode", 0,
            "socket-fd-target", "parse"});
    }
    return Result<std::optional<std::uint64_t>>::success(inode);
}

} // namespace lx::linux::procfs
