#pragma once

#include "lx/contracts/IServiceProvider.h"

#include <string>

namespace lx::application {

class ServiceService final {
public:
    explicit ServiceService(
        const contracts::IServiceProvider& serviceProvider) noexcept;

    [[nodiscard]] Result<Observation<std::vector<ServiceInfo>>> list() const;
    [[nodiscard]] Result<Observation<ServiceInfo>> inspect(
        const std::string& unit) const;
    [[nodiscard]] Result<std::optional<std::string>> unitByPid(
        pid_t pid) const;
    [[nodiscard]] Result<void> start(const std::string& unit) const;
    [[nodiscard]] Result<void> stop(const std::string& unit) const;
    [[nodiscard]] Result<void> restart(const std::string& unit) const;

    [[nodiscard]] static Result<std::string> normalizeUnit(
        const std::string& unit);

private:
    const contracts::IServiceProvider& serviceProvider_;
};

} // namespace lx::application
