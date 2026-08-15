#pragma once

#include "lx/domain/ResourceGraph.h"
#include "lx/domain/Result.h"

#include <string>

namespace lx::application {

class LogService;
class PortService;
class ProcessService;
class ResourceResolver;
class ServiceService;

class InspectService final {
public:
    InspectService(const ResourceResolver& resolver,
                   const PortService& portService,
                   const ProcessService& processService,
                   const ServiceService& serviceService,
                   const LogService& logService) noexcept;

    [[nodiscard]] Result<ResourceGraph> inspect(
        const std::string& expression) const;
    [[nodiscard]] Result<ResourceGraph> inspect(ResourceTarget target) const;

private:
    const ResourceResolver& resolver_;
    const PortService& portService_;
    const ProcessService& processService_;
    const ServiceService& serviceService_;
    const LogService& logService_;
};

} // namespace lx::application
