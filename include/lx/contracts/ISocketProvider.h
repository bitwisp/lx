#pragma once
#include "lx/domain/Observation.h"
#include "lx/domain/Result.h"
#include "lx/domain/SocketInfo.h"
#include <vector>
namespace lx::contracts {
class ISocketProvider {
public:
    virtual ~ISocketProvider() = default;
    virtual Result<Observation<std::vector<SocketInfo>>> query(const SocketQuery&) const = 0;
};
} // namespace lx::contracts
