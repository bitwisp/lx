#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace lx {

struct ProcessInfo {
    pid_t pid = -1;
    pid_t ppid = -1;
    std::string name;
    std::string state;
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    std::string user;
    std::optional<std::string> executable;
    std::optional<std::string> cwd;
    std::vector<std::string> argv;
    std::uint64_t rssBytes = 0;
    std::uint32_t threads = 0;
    std::optional<double> cpuPercent;
    std::optional<std::string> systemdUnit;
};

} // namespace lx
