#pragma once

#include "lx/domain/ProcessSignal.h"
#include "lx/domain/Result.h"

#include <chrono>
#include <sys/types.h>

namespace lx::linux::process {

class PidFd final {
public:
    static Result<PidFd> open(pid_t pid);

    ~PidFd();
    PidFd(PidFd&& other) noexcept;
    PidFd& operator=(PidFd&& other) noexcept;
    PidFd(const PidFd&) = delete;
    PidFd& operator=(const PidFd&) = delete;

    [[nodiscard]] Result<void> send(ProcessSignal signal) const;
    [[nodiscard]] Result<bool> waitForExit(
        std::chrono::milliseconds timeout) const;
    [[nodiscard]] bool valid() const noexcept;

private:
    explicit PidFd(int fd) noexcept;

    int fd_ = -1;
};

} // namespace lx::linux::process
