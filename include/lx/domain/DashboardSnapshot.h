#pragma once

#include "lx/domain/PortInfo.h"
#include "lx/domain/ProcessInfo.h"
#include "lx/domain/ServiceInfo.h"
#include "lx/domain/SystemMetrics.h"
#include "lx/domain/Warning.h"

#include <chrono>
#include <vector>

namespace lx {

struct DashboardSnapshot {
    HostStatus host;
    std::vector<ProcessInfo> processes;
    std::vector<PortInfo> ports;
    std::vector<ServiceInfo> services;
    bool hostStale = false;
    bool processesStale = false;
    bool portsStale = false;
    bool servicesStale = false;
    std::vector<Warning> warnings;
    std::chrono::system_clock::time_point capturedAt{};
};

} // namespace lx
