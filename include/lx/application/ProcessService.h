#pragma once

#include "lx/contracts/IProcessProvider.h"

namespace lx::application {

class ProcessService final {
public:
    explicit ProcessService(const contracts::IProcessProvider& provider) noexcept;

    [[nodiscard]] Result<Observation<ProcessInfo>> inspect(pid_t pid) const;

private:
    const contracts::IProcessProvider& provider_;
};

} // namespace lx::application

