#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>

namespace lx {

struct JournalEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string cursor;
    std::optional<std::string> systemdUnit;
    std::optional<pid_t> pid;
    std::optional<std::string> command;
    std::string message;
    std::optional<std::uint8_t> priority;
};

} // namespace lx
