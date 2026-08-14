#pragma once

#include "lx/domain/Result.h"

#include <string>
#include <string_view>
#include <sys/types.h>

namespace lx::linux::procfs {

struct StatRecord {
    pid_t pid = -1;
    std::string name;
    char state = '?';
    pid_t ppid = -1;
};

[[nodiscard]] Result<StatRecord> parseStat(std::string_view contents);

} // namespace lx::linux::procfs

