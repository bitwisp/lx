#pragma once

#include "lx/domain/Result.h"

#include <filesystem>
#include <string>
#include <sys/types.h>

namespace lx::linux::procfs {

class ProcFsReader final {
public:
    explicit ProcFsReader(std::filesystem::path root = "/proc");

    [[nodiscard]] Result<std::string> readFile(pid_t pid,
                                                const std::string& name) const;
    [[nodiscard]] Result<std::string> readLink(pid_t pid,
                                                const std::string& name) const;

private:
    [[nodiscard]] std::filesystem::path pathFor(pid_t pid,
                                                const std::string& name) const;

    std::filesystem::path root_;
};

} // namespace lx::linux::procfs

