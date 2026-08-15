#pragma once

#include "lx/domain/Result.h"

namespace lx::cli {

class ITuiRunner {
public:
    virtual ~ITuiRunner() = default;
    [[nodiscard]] virtual Result<void> run() = 0;
};

} // namespace lx::cli
