#pragma once

#include "lx/domain/Result.h"

#include <cstdint>
#include <string>

namespace lx::linux::procfs {

class PasswdUserResolver final {
public:
    [[nodiscard]] Result<std::string> nameForUid(std::uint32_t uid) const;
};

} // namespace lx::linux::procfs

