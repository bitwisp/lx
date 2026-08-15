#pragma once

#include "lx/domain/Result.h"
#include "lx/domain/ServiceInfo.h"

#include <systemd/sd-bus.h>
#include <optional>
#include <string>
#include <vector>

namespace lx::linux {

struct SystemdUnitRecord {
    std::string name;
    std::string description;
    std::string loadState;
    std::string activeState;
    std::string subState;
};

[[nodiscard]] std::optional<ServiceInfo> serviceFromUnitRecord(
    const SystemdUnitRecord& record);

[[nodiscard]] Result<std::vector<ServiceInfo>> parseListUnitsMessage(
    sd_bus_message* message);

} // namespace lx::linux
