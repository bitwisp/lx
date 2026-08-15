#pragma once

#include "lx/domain/Result.h"
#include "lx/domain/SystemMetrics.h"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace lx::linux::procfs {

struct MemoryRecord {
    std::uint64_t totalBytes = 0;
    std::uint64_t availableBytes = 0;
};

[[nodiscard]] Result<CpuTimes> parseCpuTimes(std::string_view contents);
[[nodiscard]] Result<MemoryRecord> parseMemoryInfo(std::string_view contents);
[[nodiscard]] Result<std::chrono::milliseconds> parseUptime(
    std::string_view contents);
[[nodiscard]] std::uint32_t countLogicalCpus(std::string_view contents);

} // namespace lx::linux::procfs
