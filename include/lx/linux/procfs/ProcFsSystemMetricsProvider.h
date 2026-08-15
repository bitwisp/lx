#pragma once

#include "lx/contracts/ISystemMetricsProvider.h"

#include <filesystem>

namespace lx::linux::procfs {

class ProcFsSystemMetricsProvider final
    : public contracts::ISystemMetricsProvider {
public:
    explicit ProcFsSystemMetricsProvider(std::filesystem::path root = "/proc");
    [[nodiscard]] Result<SystemMetricsSample> sample() const override;

private:
    std::filesystem::path root_;
};

} // namespace lx::linux::procfs
