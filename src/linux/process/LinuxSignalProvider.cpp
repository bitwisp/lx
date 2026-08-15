#include "lx/linux/process/LinuxSignalProvider.h"

#include "lx/linux/process/PidFd.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace lx::linux::process {
namespace {

Error makeError(const int number, const std::string& operation)
{
    ErrorCode code = ErrorCode::IoError;
    if (number == ESRCH) code = ErrorCode::NotFound;
    else if (number == EPERM || number == EACCES) {
        code = ErrorCode::PermissionDenied;
    }
    return {code,
            "Unable to " + operation + ": " +
                std::system_category().message(number),
            number, "linux-signal-provider", operation};
}

int nativeSignal(const ProcessSignal signal)
{
    return signal == ProcessSignal::Terminate ? SIGTERM : SIGKILL;
}

Result<SignalDelivery> sendWithKill(
    const pid_t pid, const ProcessSignal signal)
{
    if (::kill(pid, nativeSignal(signal)) == 0) {
        return Result<SignalDelivery>::success(
            {pid, signal, SignalMechanism::Kill});
    }
    return Result<SignalDelivery>::failure(makeError(errno, "send signal"));
}

Result<bool> exitedWithKill(const pid_t pid)
{
    if (::kill(pid, 0) == 0 || errno == EPERM) {
        return Result<bool>::success(false);
    }
    if (errno == ESRCH) return Result<bool>::success(true);
    return Result<bool>::failure(makeError(errno, "check process lifecycle"));
}

} // namespace

LinuxSignalProvider::LinuxSignalProvider(const PidFdPolicy pidFdPolicy) noexcept
    : pidFdPolicy_(pidFdPolicy)
{
}

Result<SignalDelivery> LinuxSignalProvider::send(
    const pid_t pid, const ProcessSignal signal) const
{
    if (pidFdPolicy_ == PidFdPolicy::Auto) {
        auto handle = PidFd::open(pid);
        if (handle) {
            const auto sent = handle.value().send(signal);
            if (sent) {
                return Result<SignalDelivery>::success(
                    {pid, signal, SignalMechanism::PidFd});
            }
            if (sent.error().code != ErrorCode::Unsupported) {
                return Result<SignalDelivery>::failure(sent.error());
            }
        } else if (handle.error().code != ErrorCode::Unsupported) {
            return Result<SignalDelivery>::failure(handle.error());
        }
    }
    return sendWithKill(pid, signal);
}

Result<bool> LinuxSignalProvider::waitForExit(
    const pid_t pid, const std::chrono::milliseconds timeout) const
{
    if (pidFdPolicy_ == PidFdPolicy::Auto) {
        auto handle = PidFd::open(pid);
        if (handle) return handle.value().waitForExit(timeout);
        if (handle.error().code == ErrorCode::NotFound) {
            return Result<bool>::success(true);
        }
        if (handle.error().code != ErrorCode::Unsupported) {
            return Result<bool>::failure(handle.error());
        }
    }

    const auto bounded = std::max(timeout, std::chrono::milliseconds{0});
    const auto deadline = std::chrono::steady_clock::now() + bounded;
    while (true) {
        const auto exited = exitedWithKill(pid);
        if (!exited || exited.value()) return exited;
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return Result<bool>::success(false);
        std::this_thread::sleep_for(std::min(
            std::chrono::milliseconds{10},
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)));
    }
}

SignalCapabilities LinuxSignalProvider::capabilities() const
{
    if (pidFdPolicy_ == PidFdPolicy::Disabled) return {true, false};
    const auto handle = PidFd::open(::getpid());
    return {true, static_cast<bool>(handle)};
}

} // namespace lx::linux::process
