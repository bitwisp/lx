#pragma once

#include "lx/contracts/IProcessProvider.h"
#include "lx/contracts/ISignalProvider.h"
#include "lx/domain/ProcessQuery.h"

#include <chrono>

namespace lx::application {

class ServiceService;

class ProcessService final {
public:
    ProcessService(const contracts::IProcessProvider& processProvider,
                   const contracts::ISignalProvider& signalProvider,
                   pid_t selfPid,
                   const ServiceService* serviceService = nullptr) noexcept;

    [[nodiscard]] Result<Observation<ProcessInfo>> inspect(pid_t pid) const;
    [[nodiscard]] Result<Observation<std::vector<ProcessInfo>>> list(
        ProcessQuery query = {}) const;
    [[nodiscard]] Result<SignalDelivery> stop(pid_t pid) const;
    [[nodiscard]] Result<SignalDelivery> kill(pid_t pid) const;
    [[nodiscard]] Result<bool> waitForExit(
        pid_t pid, std::chrono::milliseconds timeout) const;
    [[nodiscard]] Result<void> validateSignalTarget(pid_t pid) const;
    [[nodiscard]] SignalCapabilities signalCapabilities() const;

private:
    [[nodiscard]] Result<SignalDelivery> signal(
        pid_t pid, ProcessSignal processSignal) const;

    const contracts::IProcessProvider& processProvider_;
    const contracts::ISignalProvider& signalProvider_;
    pid_t selfPid_;
    const ServiceService* serviceService_;
};

} // namespace lx::application
