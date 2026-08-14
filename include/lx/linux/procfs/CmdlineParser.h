#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lx::linux::procfs {

[[nodiscard]] std::vector<std::string> parseCmdline(std::string_view contents);

} // namespace lx::linux::procfs

