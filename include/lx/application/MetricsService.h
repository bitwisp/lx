#pragma once

#include "lx/contracts/IProcessMetricsProvider.h"
#include "lx/contracts/ISystemMetricsProvider.h"
#include "lx/domain/Result.h"
#include "lx/domain/SystemMetrics.h"

#include <chrono>
#include <functional>

namespace lx::application {

class MetricsService final {
public:
    using Waiter = std::function<void(std::chrono::milliseconds)>;

    MetricsService(const contracts::ISystemMetricsProvider& system,
                   const contracts::IProcessMetricsProvider& processes,
                   Waiter waiter = {});

    [[nodiscard]] Result<MetricsSnapshot> capture() const;
    [[nodiscard]] Result<MetricsView> measure(
        std::chrono::milliseconds interval = std::chrono::milliseconds{250}) const;
    [[nodiscard]] static MetricsView compare(const MetricsSnapshot& previous,
                                             const MetricsSnapshot& current);

private:
    const contracts::ISystemMetricsProvider& system_;
    const contracts::IProcessMetricsProvider& processes_;
    Waiter waiter_;
};

} // namespace lx::application
