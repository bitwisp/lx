#pragma once
#include "lx/contracts/ISocketOwnerResolver.h"
#include "lx/contracts/ISocketProvider.h"
#include "lx/domain/PortInfo.h"
#include "lx/domain/PortRelease.h"

#include <chrono>

namespace lx::application {
class ProcessService;

class PortService final {
public:
    PortService(const contracts::ISocketProvider& socketProvider,
                const contracts::ISocketOwnerResolver& ownerResolver,
                const ProcessService& processService,
                std::chrono::milliseconds gracePeriod =
                    std::chrono::seconds{3}) noexcept;

    Result<Observation<std::vector<PortInfo>>> inspect(
        const SocketQuery& query) const;
    Result<Observation<PortReleasePlan>> prepareRelease(
        std::uint16_t localPort) const;
    Result<Observation<PortReleaseResult>> terminate(
        const PortReleasePlan& plan) const;
    Result<Observation<PortReleaseResult>> force(
        const PortReleasePlan& plan) const;

private:
    const contracts::ISocketProvider& socketProvider_;
    const contracts::ISocketOwnerResolver& ownerResolver_;
    const ProcessService& processService_;
    std::chrono::milliseconds gracePeriod_;
};
} // namespace lx::application
