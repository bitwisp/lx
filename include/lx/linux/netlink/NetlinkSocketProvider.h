#pragma once
#include "lx/contracts/ISocketProvider.h"
namespace lx::linux::netlink {
class NetlinkSocketProvider final : public contracts::ISocketProvider {
public: Result<Observation<std::vector<SocketInfo>>> query(const SocketQuery&) const override;
};
} // namespace lx::linux::netlink
