#pragma once

#include "lx/contracts/IProcessProvider.h"
#include "lx/linux/procfs/PasswdUserResolver.h"
#include "lx/linux/procfs/ProcFsReader.h"

#include <filesystem>

namespace lx::linux::procfs {

class ProcFsProcessProvider final : public contracts::IProcessProvider {
public:
    explicit ProcFsProcessProvider(std::filesystem::path root = "/proc");
    [[nodiscard]] Result<Observation<ProcessInfo>> get(pid_t pid) const override;
    [[nodiscard]] Result<Observation<std::vector<ProcessInfo>>> list() const override;

private:
    ProcFsReader reader_;
    PasswdUserResolver userResolver_;
};

} // namespace lx::linux::procfs
