#pragma once

#include "lx/contracts/ISignalProvider.h"

namespace lx::linux::process {

enum class PidFdPolicy {
    Auto,
    Disabled,
};

class LinuxSignalProvider final : public contracts::ISignalProvider {
public:
    explicit LinuxSignalProvider(
        PidFdPolicy pidFdPolicy = PidFdPolicy::Auto) noexcept;

    Result<SignalDelivery> send(
        pid_t pid, ProcessSignal signal) const override;
    Result<bool> waitForExit(
        pid_t pid, std::chrono::milliseconds timeout) const override;
    [[nodiscard]] SignalCapabilities capabilities() const override;

private:
    PidFdPolicy pidFdPolicy_;
};

} // namespace lx::linux::process
