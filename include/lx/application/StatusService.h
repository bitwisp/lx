#pragma once

#include "lx/application/MetricsService.h"

#include <chrono>

namespace lx::application {

class StatusService final {
public:
    explicit StatusService(const MetricsService& metrics);
    [[nodiscard]] Result<HostStatus> get(
        std::chrono::milliseconds interval = std::chrono::milliseconds{250}) const;
    [[nodiscard]] Result<MetricsView> measure(
        std::chrono::milliseconds interval = std::chrono::milliseconds{250}) const;

private:
    const MetricsService& metrics_;
};

} // namespace lx::application
