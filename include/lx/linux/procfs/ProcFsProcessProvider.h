#pragma once

#include "lx/contracts/IProcessProvider.h"
#include "lx/contracts/IProcessMetricsProvider.h"
#include "lx/linux/procfs/PasswdUserResolver.h"
#include "lx/linux/procfs/ProcFsReader.h"

#include <filesystem>

namespace lx::linux::procfs {

class ProcFsProcessProvider final : public contracts::IProcessProvider,
                                    public contracts::IProcessMetricsProvider {
public:
    explicit ProcFsProcessProvider(std::filesystem::path root = "/proc");
    [[nodiscard]] Result<Observation<ProcessInfo>> get(pid_t pid) const override;
    [[nodiscard]] Result<Observation<std::vector<ProcessInfo>>> list() const override;
    [[nodiscard]] Result<std::vector<ProcessCpuSample>> sample() const override;

private:
    ProcFsReader reader_;
    PasswdUserResolver userResolver_;
};

} // namespace lx::linux::procfs
