#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <sys/types.h>

namespace lx {

struct LogQuery {
    std::optional<std::string> unit;
    std::optional<pid_t> pid;
    std::optional<std::chrono::system_clock::time_point> since;
    std::size_t limit = 50;
    bool follow = false;
};

} // namespace lx
