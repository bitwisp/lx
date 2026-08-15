#pragma once
#include "lx/contracts/IProcessProvider.h"
#include "lx/contracts/ISocketOwnerResolver.h"
#include "lx/contracts/ISocketProvider.h"
#include "lx/domain/PortInfo.h"

namespace lx::application {
class PortService final {
public:
    PortService(const contracts::ISocketProvider& socketProvider,
                const contracts::ISocketOwnerResolver& ownerResolver,
                const contracts::IProcessProvider& processProvider) noexcept;

    Result<Observation<std::vector<PortInfo>>> inspect(
        const SocketQuery& query) const;

private:
    const contracts::ISocketProvider& socketProvider_;
    const contracts::ISocketOwnerResolver& ownerResolver_;
    const contracts::IProcessProvider& processProvider_;
};
} // namespace lx::application
