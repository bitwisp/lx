#pragma once

#include "lx/domain/Error.h"

#include <string>

namespace lx {

struct Warning {
    ErrorCode code = ErrorCode::OperationFailed;
    std::string message;
    int systemError = 0;
    std::string component;
    std::string operation;
};

} // namespace lx

