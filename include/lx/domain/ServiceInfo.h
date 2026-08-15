#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>

namespace lx {

struct ServiceInfo {
    std::string unitName;
    std::string description;
    std::string loadState;
    std::string activeState;
    std::string subState;
    std::string unitFileState;
    std::optional<pid_t> mainPid;
    std::uint64_t activeEnterTimestampUsec = 0;
};

} // namespace lx
