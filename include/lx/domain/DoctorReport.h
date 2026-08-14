#pragma once

#include <string>
#include <vector>

namespace lx {

enum class CapabilityStatus {
    Available,
    NotImplemented,
};

struct CapabilityCheck {
    std::string name;
    CapabilityStatus status = CapabilityStatus::NotImplemented;
    std::string detail;
};

struct DoctorReport {
    std::vector<CapabilityCheck> checks;
};

} // namespace lx

