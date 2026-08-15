#include "lx/application/PortService.h"
namespace lx::application {
PortService::PortService(const contracts::ISocketProvider& provider) noexcept : provider_(provider) {}
Result<Observation<std::vector<SocketInfo>>> PortService::query(const SocketQuery& query) const { return provider_.query(query); }
} // namespace lx::application
