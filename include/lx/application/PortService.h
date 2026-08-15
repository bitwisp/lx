#pragma once
#include "lx/contracts/ISocketProvider.h"
namespace lx::application {
class PortService final {
public:
    explicit PortService(const contracts::ISocketProvider& provider) noexcept;
    Result<Observation<std::vector<SocketInfo>>> query(const SocketQuery& query) const;
private: const contracts::ISocketProvider& provider_;
};
} // namespace lx::application
