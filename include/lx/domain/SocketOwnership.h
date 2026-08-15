#pragma once

#include <cstdint>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

namespace lx {

using SocketOwnership =
    std::unordered_map<std::uint64_t, std::vector<pid_t>>;

} // namespace lx
