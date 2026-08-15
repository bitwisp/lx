#pragma once

#include "lx/domain/PortInfo.h"
#include "lx/domain/ServiceInfo.h"
#include "lx/domain/Warning.h"

#include <string>
#include <vector>

namespace lx {

struct FindResult {
    std::vector<ServiceInfo> services;
    std::vector<ProcessInfo> processes;
    std::vector<PortInfo> ports;
    std::vector<std::string> executables;
    std::vector<Warning> warnings;
};

} // namespace lx
