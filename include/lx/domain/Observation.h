#pragma once

#include "lx/domain/Warning.h"

#include <vector>

namespace lx {

template <typename T>
struct Observation {
    T value;
    std::vector<Warning> warnings;
};

} // namespace lx

