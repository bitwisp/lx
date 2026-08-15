#pragma once

#include "lx/domain/DashboardSnapshot.h"

namespace lx::contracts {

class IDashboardProvider {
public:
    virtual ~IDashboardProvider() = default;
    [[nodiscard]] virtual DashboardSnapshot refresh() = 0;
};

} // namespace lx::contracts
