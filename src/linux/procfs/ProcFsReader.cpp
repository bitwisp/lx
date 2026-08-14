#include "lx/linux/procfs/ProcFsReader.h"

#include <cerrno>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace lx::linux::procfs {
namespace {

Error makeError(const int number, const std::string& operation)
{
    ErrorCode code = ErrorCode::IoError;
    if (number == ENOENT || number == ESRCH) {
        code = ErrorCode::NotFound;
    } else if (number == EACCES || number == EPERM) {
        code = ErrorCode::PermissionDenied;
    }
    return {code,
            "Unable to " + operation + ": " +
                std::system_category().message(number),
            number, "procfs", operation};
}

} // namespace

ProcFsReader::ProcFsReader(std::filesystem::path root)
    : root_(std::move(root))
{
}

Result<std::string> ProcFsReader::readFile(
    const pid_t pid, const std::string& name) const
{
    errno = 0;
    std::ifstream input(pathFor(pid, name), std::ios::binary);
    if (!input) {
        const auto number = errno == 0 ? EIO : errno;
        return Result<std::string>::failure(makeError(number, "read " + name));
    }

    std::string contents{std::istreambuf_iterator<char>{input},
                         std::istreambuf_iterator<char>{}};
    if (input.bad()) {
        const auto number = errno == 0 ? EIO : errno;
        return Result<std::string>::failure(makeError(number, "read " + name));
    }
    return Result<std::string>::success(std::move(contents));
}

Result<std::string> ProcFsReader::readLink(
    const pid_t pid, const std::string& name) const
{
    std::error_code error;
    auto target = std::filesystem::read_symlink(pathFor(pid, name), error);
    if (error) {
        return Result<std::string>::failure(
            makeError(error.value(), "readlink " + name));
    }
    return Result<std::string>::success(target.string());
}

std::filesystem::path ProcFsReader::pathFor(
    const pid_t pid, const std::string& name) const
{
    return root_ / std::to_string(pid) / name;
}

} // namespace lx::linux::procfs
