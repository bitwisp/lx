#pragma once

#include "lx/domain/Error.h"
#include "lx/domain/Warning.h"

#include <string>
#include <vector>

namespace lx::cli {

class JsonSerializer final {
public:
    static constexpr int schemaVersion = 1;

    [[nodiscard]] static std::string error(
        const std::string& command, const std::string& operation,
        const Error& value);

    [[nodiscard]] static std::string emptySuccess(
        const std::string& command, const std::string& operation,
        const std::vector<Warning>& warnings = {});
};

} // namespace lx::cli
