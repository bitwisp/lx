#pragma once

#include "lx/domain/Observation.h"
#include "lx/domain/Result.h"
#include "lx/domain/SocketOwnership.h"

#include <cstdint>
#include <vector>

namespace lx::contracts {

class ISocketOwnerResolver {
public:
    virtual ~ISocketOwnerResolver() = default;

    virtual Result<Observation<SocketOwnership>> resolve(
        const std::vector<std::uint64_t>& inodes) const = 0;
};

} // namespace lx::contracts
