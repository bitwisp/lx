#pragma once

#include "lx/domain/Result.h"
#include "lx/domain/SystemMetrics.h"

namespace lx::contracts {

class ISystemMetricsProvider {
public:
    virtual ~ISystemMetricsProvider() = default;
    [[nodiscard]] virtual Result<SystemMetricsSample> sample() const = 0;
};

} // namespace lx::contracts
