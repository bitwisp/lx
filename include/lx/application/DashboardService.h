#pragma once

#include "lx/application/MetricsService.h"
#include "lx/domain/DashboardSnapshot.h"
#include "lx/contracts/IDashboardProvider.h"

#include <chrono>
#include <optional>

namespace lx::application {

class PortService;
class ProcessService;
class ServiceService;

class DashboardService final : public contracts::IDashboardProvider {
public:
    DashboardService(const MetricsService& metrics,
                     const ProcessService& processes,
                     const PortService& ports,
                     const ServiceService& services);

    [[nodiscard]] DashboardSnapshot refresh() override;
    [[nodiscard]] DashboardSnapshot refreshAt(
        std::chrono::steady_clock::time_point now);

private:
    const MetricsService& metrics_;
    const ProcessService& processes_;
    const PortService& ports_;
    const ServiceService& services_;
    std::optional<MetricsSnapshot> previousMetrics_;
    std::optional<DashboardSnapshot> previous_;
    std::optional<std::chrono::steady_clock::time_point> servicesRefreshedAt_;
};

} // namespace lx::application
