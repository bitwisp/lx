#include "lx/application/MetricsService.h"

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <utility>

namespace lx::application {

MetricsService::MetricsService(const contracts::ISystemMetricsProvider& system,
                               const contracts::IProcessMetricsProvider& processes,
                               Waiter waiter)
    : system_(system), processes_(processes), waiter_(std::move(waiter))
{
    if (!waiter_) waiter_ = [](const auto duration) { std::this_thread::sleep_for(duration); };
}

Result<MetricsSnapshot> MetricsService::capture() const
{
    auto system = system_.sample();
    if (!system) return Result<MetricsSnapshot>::failure(system.error());
    auto processes = processes_.sample();
    if (!processes) return Result<MetricsSnapshot>::failure(processes.error());
    return Result<MetricsSnapshot>::success(
        {std::move(system.value()), std::move(processes.value())});
}

Result<MetricsView> MetricsService::measure(const std::chrono::milliseconds interval) const
{
    auto previous = capture();
    if (!previous) return Result<MetricsView>::failure(previous.error());
    waiter_(interval);
    auto current = capture();
    if (!current) return Result<MetricsView>::failure(current.error());
    return Result<MetricsView>::success(compare(previous.value(), current.value()));
}

MetricsView MetricsService::compare(const MetricsSnapshot& previous,
                                    const MetricsSnapshot& current)
{
    MetricsView view;
    view.host.hostname = current.system.hostname;
    view.host.memoryTotalBytes = current.system.memoryTotalBytes;
    view.host.memoryUsedBytes = current.system.memoryAvailableBytes <=
                                       current.system.memoryTotalBytes
                                   ? current.system.memoryTotalBytes -
                                         current.system.memoryAvailableBytes
                                   : 0;
    view.host.uptime = current.system.uptime;

    if (current.system.cpu.totalTicks > previous.system.cpu.totalTicks &&
        current.system.cpu.idleTicks >= previous.system.cpu.idleTicks) {
        const auto total = current.system.cpu.totalTicks - previous.system.cpu.totalTicks;
        const auto idle = current.system.cpu.idleTicks - previous.system.cpu.idleTicks;
        if (idle <= total) {
            view.host.cpuPercent = std::clamp(
                100.0 * static_cast<double>(total - idle) /
                    static_cast<double>(total),
                0.0, 100.0);
        }

        std::unordered_map<pid_t, ProcessCpuSample> old;
        old.reserve(previous.processes.size());
        for (const auto& process : previous.processes) old[process.pid] = process;
        const auto maximum = 100.0 * std::max<std::uint32_t>(1, current.system.logicalCpuCount);
        for (const auto& process : current.processes) {
            const auto found = old.find(process.pid);
            if (found == old.end() ||
                found->second.startTimeTicks != process.startTimeTicks ||
                process.cpuTimeTicks < found->second.cpuTimeTicks) {
                continue;
            }
            const auto used = process.cpuTimeTicks - found->second.cpuTimeTicks;
            view.processCpuPercent[process.pid] = std::clamp(
                100.0 * static_cast<double>(used) *
                    std::max<std::uint32_t>(1, current.system.logicalCpuCount) /
                    static_cast<double>(total),
                0.0, maximum);
        }
    }
    return view;
}

} // namespace lx::application
