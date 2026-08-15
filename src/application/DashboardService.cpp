#include "lx/application/DashboardService.h"

#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <utility>

namespace lx::application {
namespace {

Warning warningFrom(const Error& error)
{
    return {error.code, error.message, error.systemError, error.component,
            error.operation};
}

void attachCpu(std::vector<ProcessInfo>& processes, const MetricsView& metrics)
{
    for (auto& process : processes) {
        const auto found = metrics.processCpuPercent.find(process.pid);
        process.cpuPercent = found == metrics.processCpuPercent.end()
                                 ? std::optional<double>{}
                                 : std::optional<double>{found->second};
    }
}

} // namespace

DashboardService::DashboardService(const MetricsService& metrics,
                                   const ProcessService& processes,
                                   const PortService& ports,
                                   const ServiceService& services)
    : metrics_(metrics), processes_(processes), ports_(ports), services_(services)
{
}

DashboardSnapshot DashboardService::refresh()
{
    return refreshAt(std::chrono::steady_clock::now());
}

DashboardSnapshot DashboardService::refreshAt(
    const std::chrono::steady_clock::time_point now)
{
    DashboardSnapshot next = previous_.value_or(DashboardSnapshot{});
    next.warnings.clear();
    next.capturedAt = std::chrono::system_clock::now();

    std::optional<MetricsView> metricsView;
    auto metrics = metrics_.capture();
    if (metrics) {
        metricsView = MetricsService::compare(
            previousMetrics_.value_or(metrics.value()), metrics.value());
        next.host = metricsView->host;
        next.hostStale = false;
        previousMetrics_ = std::move(metrics.value());
    } else {
        next.hostStale = true;
        next.warnings.push_back(warningFrom(metrics.error()));
    }

    auto processes = processes_.list();
    if (processes) {
        next.processes = std::move(processes.value().value);
        if (metricsView) attachCpu(next.processes, *metricsView);
        next.processesStale = false;
        next.warnings.insert(next.warnings.end(),
                             processes.value().warnings.begin(),
                             processes.value().warnings.end());
    } else {
        next.processesStale = true;
        next.warnings.push_back(warningFrom(processes.error()));
    }

    auto ports = ports_.inspect({});
    if (ports) {
        next.ports = std::move(ports.value().value);
        next.portsStale = false;
        next.warnings.insert(next.warnings.end(), ports.value().warnings.begin(),
                             ports.value().warnings.end());
    } else {
        next.portsStale = true;
        next.warnings.push_back(warningFrom(ports.error()));
    }

    constexpr auto serviceInterval = std::chrono::seconds{3};
    const bool refreshServices = !servicesRefreshedAt_ ||
                                 now - *servicesRefreshedAt_ >= serviceInterval;
    if (refreshServices) {
        auto services = services_.list();
        if (services) {
            next.services = std::move(services.value().value);
            next.servicesStale = false;
            next.warnings.insert(next.warnings.end(),
                                 services.value().warnings.begin(),
                                 services.value().warnings.end());
            servicesRefreshedAt_ = now;
        } else {
            next.servicesStale = true;
            next.warnings.push_back(warningFrom(services.error()));
        }
    }

    previous_ = next;
    return next;
}

} // namespace lx::application
