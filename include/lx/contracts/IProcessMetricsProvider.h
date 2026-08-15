#pragma once

#include "lx/domain/Result.h"
#include "lx/domain/SystemMetrics.h"

#include <vector>

namespace lx::contracts {

class IProcessMetricsProvider {
public:
    virtual ~IProcessMetricsProvider() = default;
    [[nodiscard]] virtual Result<std::vector<ProcessCpuSample>> sample() const = 0;
};

} // namespace lx::contracts
