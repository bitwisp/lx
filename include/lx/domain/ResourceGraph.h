#pragma once

#include "lx/domain/JournalEntry.h"
#include "lx/domain/PortInfo.h"
#include "lx/domain/ResourceTarget.h"
#include "lx/domain/ServiceInfo.h"
#include "lx/domain/Warning.h"

#include <vector>

namespace lx {

struct ResourceGraph {
    ResourceTarget root;
    std::vector<PortInfo> ports;
    std::vector<ProcessInfo> processes;
    std::vector<ServiceInfo> services;
    std::vector<JournalEntry> recentLogs;
    std::vector<Warning> warnings;
};

} // namespace lx
