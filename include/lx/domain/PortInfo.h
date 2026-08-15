#pragma once

#include "lx/domain/ProcessInfo.h"
#include "lx/domain/SocketInfo.h"

#include <vector>

namespace lx {

struct PortInfo {
    SocketInfo socket;
    std::vector<ProcessInfo> owners;
};

} // namespace lx
