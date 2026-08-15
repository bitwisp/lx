#include "lx/linux/procfs/SocketInodeResolver.h"

#include "lx/linux/procfs/SocketFdTarget.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace lx::linux::procfs {
namespace {

ErrorCode errorCode(const std::error_code& error)
{
    if (error == std::errc::permission_denied ||
        error == std::errc::operation_not_permitted) {
        return ErrorCode::PermissionDenied;
    }
    if (error == std::errc::no_such_file_or_directory ||
        error == std::errc::no_such_process) {
        return ErrorCode::NotFound;
    }
    return ErrorCode::IoError;
}

Error makeError(const std::error_code& error, const std::string& operation)
{
    return {errorCode(error),
            "Unable to " + operation + ": " + error.message(),
            error.value(), "socket-inode-resolver", operation};
}

std::optional<pid_t> parsePid(const std::string& name)
{
    unsigned long value = 0;
    const auto parsed = std::from_chars(
        name.data(), name.data() + name.size(), value);
    if (name.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != name.data() + name.size() || value == 0 ||
        value > static_cast<unsigned long>(std::numeric_limits<pid_t>::max())) {
        return std::nullopt;
    }
    return static_cast<pid_t>(value);
}

bool disappeared(const std::error_code& error)
{
    return error == std::errc::no_such_file_or_directory ||
           error == std::errc::no_such_process;
}

bool denied(const std::error_code& error)
{
    return error == std::errc::permission_denied ||
           error == std::errc::operation_not_permitted;
}

} // namespace

SocketInodeResolver::SocketInodeResolver(std::filesystem::path root)
    : root_(std::move(root))
{
}

Result<Observation<SocketOwnership>> SocketInodeResolver::resolve(
    const std::vector<std::uint64_t>& inodes) const
{
    std::unordered_set<std::uint64_t> targets;
    for (const auto inode : inodes) {
        if (inode != 0) targets.insert(inode);
    }
    if (targets.empty()) {
        return Result<Observation<SocketOwnership>>::success({{}, {}});
    }

    std::error_code error;
    std::filesystem::directory_iterator processes(root_, error);
    if (error) {
        return Result<Observation<SocketOwnership>>::failure(
            makeError(error, "enumerate procfs root"));
    }

    SocketOwnership ownership;
    std::size_t deniedCount = 0;
    std::size_t unreadableCount = 0;
    for (const auto& process : processes) {
        const auto pid = parsePid(process.path().filename().string());
        if (!pid) continue;

        error.clear();
        std::filesystem::directory_iterator descriptors(
            process.path() / "fd", error);
        if (error) {
            if (denied(error)) ++deniedCount;
            else if (!disappeared(error)) ++unreadableCount;
            continue;
        }

        for (const auto& descriptor : descriptors) {
            error.clear();
            const auto target = std::filesystem::read_symlink(
                descriptor.path(), error);
            if (error) {
                if (denied(error)) ++deniedCount;
                else if (!disappeared(error)) ++unreadableCount;
                continue;
            }
            const auto parsed = parseSocketFdTarget(target.string());
            if (!parsed || !parsed.value() ||
                targets.find(*parsed.value()) == targets.end()) {
                continue;
            }
            ownership[*parsed.value()].push_back(*pid);
        }
    }

    for (auto& [inode, owners] : ownership) {
        static_cast<void>(inode);
        std::sort(owners.begin(), owners.end());
        owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
    }

    Observation<SocketOwnership> observation{std::move(ownership), {}};
    if (deniedCount != 0) {
        observation.warnings.push_back({
            ErrorCode::PermissionDenied,
            std::to_string(deniedCount) +
                " procfs entries could not be inspected because of permissions",
            0, "socket-inode-resolver", "resolve"});
    }
    if (unreadableCount != 0) {
        observation.warnings.push_back({
            ErrorCode::IoError,
            std::to_string(unreadableCount) +
                " procfs entries could not be inspected",
            0, "socket-inode-resolver", "resolve"});
    }
    return Result<Observation<SocketOwnership>>::success(std::move(observation));
}

} // namespace lx::linux::procfs
