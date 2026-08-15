#pragma once

#include "lx/domain/ResourceTarget.h"
#include "lx/domain/Result.h"

#include <string>

namespace lx::application {

class PortService;
class ProcessService;
class ServiceService;

class ResourceResolver final {
public:
    ResourceResolver(const PortService& portService,
                     const ProcessService& processService,
                     const ServiceService& serviceService) noexcept;

    [[nodiscard]] Result<ResourceTarget> resolve(
        const std::string& expression) const;

private:
    const PortService& portService_;
    const ProcessService& processService_;
    const ServiceService& serviceService_;
};

} // namespace lx::application
