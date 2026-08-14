#include "lx/linux/procfs/PasswdUserResolver.h"

#include <cerrno>
#include <pwd.h>
#include <unistd.h>
#include <vector>

namespace lx::linux::procfs {

Result<std::string> PasswdUserResolver::nameForUid(const std::uint32_t uid) const
{
    const auto suggested = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    std::size_t size = suggested > 0 ? static_cast<std::size_t>(suggested) : 1024;
    constexpr std::size_t maximum = 1024 * 1024;
    while (size <= maximum) {
        std::vector<char> buffer(size);
        passwd entry{};
        passwd* found = nullptr;
        const auto status = ::getpwuid_r(static_cast<uid_t>(uid), &entry,
                                         buffer.data(), buffer.size(), &found);
        if (status == 0 && found != nullptr) {
            return Result<std::string>::success(found->pw_name);
        }
        if (status == 0) {
            return Result<std::string>::failure({
                ErrorCode::NotFound, "No passwd entry for UID", 0,
                "passwd-user-resolver", "resolve uid"});
        }
        if (status != ERANGE) {
            return Result<std::string>::failure({
                ErrorCode::IoError, "Unable to resolve UID", status,
                "passwd-user-resolver", "resolve uid"});
        }
        size *= 2;
    }
    return Result<std::string>::failure({
        ErrorCode::OperationFailed, "Passwd entry exceeds safety limit", ERANGE,
        "passwd-user-resolver", "resolve uid"});
}

} // namespace lx::linux::procfs

