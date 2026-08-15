#pragma once

#include "lx/contracts/IServiceProvider.h"

namespace lx::linux {

class SystemdServiceProvider final : public contracts::IServiceProvider {
public:
    Result<void> probe() const override;
    Result<Observation<std::vector<ServiceInfo>>> list() const override;
    Result<Observation<ServiceInfo>> get(
        const std::string& unit) const override;
    Result<std::optional<std::string>> unitByPid(pid_t pid) const override;
    Result<void> start(const std::string& unit) const override;
    Result<void> stop(const std::string& unit) const override;
    Result<void> restart(const std::string& unit) const override;
};

} // namespace lx::linux
