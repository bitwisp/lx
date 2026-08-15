#include "lx/linux/process/PidFd.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdint>
#include <poll.h>
#include <string>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace lx::linux::process {
namespace {

Error makeError(const int number, const std::string& operation)
{
    ErrorCode code = ErrorCode::IoError;
    if (number == ENOSYS || number == EINVAL) {
        code = ErrorCode::Unsupported;
    } else if (number == ESRCH || number == ENOENT) {
        code = ErrorCode::NotFound;
    } else if (number == EPERM || number == EACCES) {
        code = ErrorCode::PermissionDenied;
    }
    return {code,
            "Unable to " + operation + ": " +
                std::system_category().message(number),
            number, "pidfd", operation};
}

int nativeSignal(const ProcessSignal signal)
{
    return signal == ProcessSignal::Terminate ? SIGTERM : SIGKILL;
}

} // namespace

Result<PidFd> PidFd::open(const pid_t pid)
{
#if defined(SYS_pidfd_open)
    const auto fd = static_cast<int>(::syscall(SYS_pidfd_open, pid, 0U));
    if (fd >= 0) return Result<PidFd>::success(PidFd{fd});
    return Result<PidFd>::failure(makeError(errno, "open process handle"));
#else
    static_cast<void>(pid);
    return Result<PidFd>::failure({
        ErrorCode::Unsupported, "pidfd_open is unavailable at build time",
        ENOSYS, "pidfd", "open process handle"});
#endif
}

PidFd::PidFd(const int fd) noexcept : fd_(fd) {}

PidFd::~PidFd()
{
    if (fd_ >= 0) ::close(fd_);
}

PidFd::PidFd(PidFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

PidFd& PidFd::operator=(PidFd&& other) noexcept
{
    if (this == &other) return *this;
    if (fd_ >= 0) ::close(fd_);
    fd_ = std::exchange(other.fd_, -1);
    return *this;
}

Result<void> PidFd::send(const ProcessSignal signal) const
{
#if defined(SYS_pidfd_send_signal)
    if (::syscall(SYS_pidfd_send_signal, fd_, nativeSignal(signal), nullptr,
                  0U) == 0) {
        return Result<void>::success();
    }
    return Result<void>::failure(makeError(errno, "send process signal"));
#else
    static_cast<void>(signal);
    return Result<void>::failure({
        ErrorCode::Unsupported,
        "pidfd_send_signal is unavailable at build time", ENOSYS, "pidfd",
        "send process signal"});
#endif
}

Result<bool> PidFd::waitForExit(
    const std::chrono::milliseconds timeout) const
{
    const auto bounded = timeout.count() > INT_MAX
                             ? INT_MAX
                             : static_cast<int>(std::max<std::int64_t>(
                                   timeout.count(), 0));
    pollfd descriptor{fd_, POLLIN, 0};
    int status = 0;
    do {
        status = ::poll(&descriptor, 1, bounded);
    } while (status < 0 && errno == EINTR);
    if (status > 0) return Result<bool>::success(true);
    if (status == 0) return Result<bool>::success(false);
    return Result<bool>::failure(makeError(errno, "wait for process exit"));
}

bool PidFd::valid() const noexcept
{
    return fd_ >= 0;
}

} // namespace lx::linux::process
