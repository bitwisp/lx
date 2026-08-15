#pragma once

#include "lx/domain/Error.h"
#include "lx/domain/Warning.h"
#include "lx/domain/Observation.h"
#include "lx/domain/ProcessInfo.h"
#include "lx/domain/PortInfo.h"
#include "lx/domain/ServiceInfo.h"

#include <string>
#include <vector>

namespace lx::cli {

class JsonSerializer final {
public:
    static constexpr int schemaVersion = 1;

    [[nodiscard]] static std::string error(
        const std::string& command, const std::string& operation,
        const Error& value);

    [[nodiscard]] static std::string emptySuccess(
        const std::string& command, const std::string& operation,
        const std::vector<Warning>& warnings = {});
    [[nodiscard]] static std::string process(
        const Observation<ProcessInfo>& value, bool rawCommand = false);
    [[nodiscard]] static std::string processes(
        const Observation<std::vector<ProcessInfo>>& value);
    [[nodiscard]] static std::string ports(
        const Observation<std::vector<PortInfo>>& value);
    [[nodiscard]] static std::string service(
        const Observation<ServiceInfo>& value);
    [[nodiscard]] static std::string services(
        const Observation<std::vector<ServiceInfo>>& value);
};

} // namespace lx::cli
