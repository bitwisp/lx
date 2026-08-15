#include "lx/application/StatusService.h"

#include <utility>

namespace lx::application {

StatusService::StatusService(const MetricsService& metrics) : metrics_(metrics) {}

Result<HostStatus> StatusService::get(const std::chrono::milliseconds interval) const
{
    auto measured = metrics_.measure(interval);
    if (!measured) return Result<HostStatus>::failure(measured.error());
    return Result<HostStatus>::success(std::move(measured.value().host));
}

Result<MetricsView> StatusService::measure(
    const std::chrono::milliseconds interval) const
{
    return metrics_.measure(interval);
}

} // namespace lx::application
