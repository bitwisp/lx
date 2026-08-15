#pragma once

#include <optional>
#include <string>

namespace lx {

struct ProcessQuery {
    std::optional<std::string> name;
    std::optional<std::string> user;
    std::optional<std::string> systemdUnit;
};

} // namespace lx
