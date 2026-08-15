#pragma once

#include "lx/domain/ProcessSignal.h"
#include "lx/domain/Result.h"

#include <chrono>
#include <sys/types.h>

namespace lx::contracts {

class ISignalProvider {
public:
    virtual ~ISignalProvider() = default;

    virtual Result<SignalDelivery> send(
        pid_t pid, ProcessSignal signal) const = 0;
    virtual Result<bool> waitForExit(
        pid_t pid, std::chrono::milliseconds timeout) const = 0;
    [[nodiscard]] virtual SignalCapabilities capabilities() const = 0;
};

} // namespace lx::contracts
