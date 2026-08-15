#include "lx/linux/process/LinuxSignalProvider.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace {

class ChildProcess final {
public:
    static ChildProcess spawn()
    {
        const auto pid = ::fork();
        if (pid == 0) {
            while (true) ::pause();
        }
        return ChildProcess{pid};
    }

    ~ChildProcess()
    {
        if (pid_ > 0 && !reaped_) {
            ::kill(pid_, SIGKILL);
            int status = 0;
            while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {}
        }
    }

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    [[nodiscard]] pid_t pid() const noexcept { return pid_; }

    int reap()
    {
        int status = 0;
        while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {}
        reaped_ = true;
        return status;
    }

private:
    explicit ChildProcess(const pid_t pid) : pid_(pid) {}

    pid_t pid_ = -1;
    bool reaped_ = false;
};

} // namespace

TEST_CASE("LinuxSignalProvider terminates a controlled child")
{
    auto child = ChildProcess::spawn();
    REQUIRE(child.pid() > 1);
    const lx::linux::process::LinuxSignalProvider provider;

    const auto delivery = provider.send(
        child.pid(), lx::ProcessSignal::Terminate);
    REQUIRE(delivery);
    const auto exited = provider.waitForExit(
        child.pid(), std::chrono::seconds{1});
    REQUIRE(exited);
    REQUIRE(exited.value());

    const auto status = child.reap();
    REQUIRE(WIFSIGNALED(status));
    REQUIRE(WTERMSIG(status) == SIGTERM);
}

TEST_CASE("LinuxSignalProvider kill fallback terminates a controlled child")
{
    auto child = ChildProcess::spawn();
    REQUIRE(child.pid() > 1);
    const lx::linux::process::LinuxSignalProvider provider{
        lx::linux::process::PidFdPolicy::Disabled};

    const auto delivery = provider.send(child.pid(), lx::ProcessSignal::Kill);
    REQUIRE(delivery);
    REQUIRE(delivery.value().mechanism == lx::SignalMechanism::Kill);
    const auto exited = provider.waitForExit(
        child.pid(), std::chrono::seconds{1});
    REQUIRE(exited);
    REQUIRE(exited.value());

    const auto status = child.reap();
    REQUIRE(WIFSIGNALED(status));
    REQUIRE(WTERMSIG(status) == SIGKILL);
}
