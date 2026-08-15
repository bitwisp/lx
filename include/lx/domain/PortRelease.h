#pragma once

#include "lx/domain/PortInfo.h"
#include "lx/domain/ProcessSignal.h"

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace lx {

struct PortReleasePlan {
    std::uint16_t localPort = 0;
    std::vector<PortInfo> ports;
    std::vector<pid_t> ownerPids;
    std::optional<std::string> recommendedUnit;
};

struct PortReleaseResult {
    bool released = false;
    std::vector<SignalDelivery> deliveries;
    std::optional<PortReleasePlan> remaining;
};

} // namespace lx
