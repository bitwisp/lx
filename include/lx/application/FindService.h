#pragma once

#include "lx/domain/FindResult.h"
#include "lx/domain/Result.h"

#include <string>

namespace lx::application {

class PortService;
class ProcessService;
class ServiceService;

class FindService final {
public:
    FindService(const PortService& portService,
                const ProcessService& processService,
                const ServiceService& serviceService) noexcept;

    [[nodiscard]] Result<FindResult> find(const std::string& query) const;

private:
    const PortService& portService_;
    const ProcessService& processService_;
    const ServiceService& serviceService_;
};

} // namespace lx::application
