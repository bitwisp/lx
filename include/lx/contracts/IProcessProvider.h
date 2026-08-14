#pragma once

#include "lx/domain/Observation.h"
#include "lx/domain/ProcessInfo.h"
#include "lx/domain/Result.h"

#include <sys/types.h>

namespace lx::contracts {

class IProcessProvider {
public:
    virtual ~IProcessProvider() = default;

    virtual Result<Observation<ProcessInfo>> get(pid_t pid) const = 0;
};

} // namespace lx::contracts

