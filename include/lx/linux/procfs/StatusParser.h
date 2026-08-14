#pragma once

#include "lx/domain/Result.h"

#include <cstdint>
#include <string_view>

namespace lx::linux::procfs {

struct StatusRecord {
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    std::uint32_t threads = 0;
    std::uint64_t rssBytes = 0;
};

[[nodiscard]] Result<StatusRecord> parseStatus(std::string_view contents);

} // namespace lx::linux::procfs

