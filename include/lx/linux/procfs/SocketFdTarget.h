#pragma once

#include "lx/domain/Result.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace lx::linux::procfs {

[[nodiscard]] Result<std::optional<std::uint64_t>> parseSocketFdTarget(
    std::string_view target);

} // namespace lx::linux::procfs
