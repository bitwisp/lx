#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

namespace lx {

struct CpuTimes {
    std::uint64_t totalTicks = 0;
    std::uint64_t idleTicks = 0;
};

struct SystemMetricsSample {
    CpuTimes cpu;
    std::uint64_t memoryTotalBytes = 0;
    std::uint64_t memoryAvailableBytes = 0;
    std::chrono::milliseconds uptime{0};
    std::uint32_t logicalCpuCount = 1;
    std::string hostname;
};

struct ProcessCpuSample {
    pid_t pid = -1;
    std::uint64_t startTimeTicks = 0;
    std::uint64_t cpuTimeTicks = 0;
};

struct MetricsSnapshot {
    SystemMetricsSample system;
    std::vector<ProcessCpuSample> processes;
};

struct HostStatus {
    std::string hostname;
    std::optional<double> cpuPercent;
    std::uint64_t memoryTotalBytes = 0;
    std::uint64_t memoryUsedBytes = 0;
    std::chrono::milliseconds uptime{0};
};

struct MetricsView {
    HostStatus host;
    std::unordered_map<pid_t, double> processCpuPercent;
};

} // namespace lx
