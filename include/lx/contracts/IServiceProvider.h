#pragma once

#include "lx/domain/Observation.h"
#include "lx/domain/Result.h"
#include "lx/domain/ServiceInfo.h"

#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace lx::contracts {

class IServiceProvider {
public:
    virtual ~IServiceProvider() = default;

    virtual Result<void> probe() const = 0;
    virtual Result<Observation<std::vector<ServiceInfo>>> list() const = 0;
    virtual Result<Observation<ServiceInfo>> get(
        const std::string& unit) const = 0;
    virtual Result<std::optional<std::string>> unitByPid(pid_t pid) const = 0;
    virtual Result<void> start(const std::string& unit) const = 0;
    virtual Result<void> stop(const std::string& unit) const = 0;
    virtual Result<void> restart(const std::string& unit) const = 0;
};

} // namespace lx::contracts
