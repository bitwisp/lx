#pragma once

#include <string>

namespace lx {

enum class ErrorCode {
    InvalidArgument,
    NotFound,
    PermissionDenied,
    Unsupported,
    Unavailable,
    IoError,
    ProtocolError,
    ParseError,
    OperationFailed,
    Timeout,
    Conflict,
    Interrupted,
};

struct Error {
    ErrorCode code = ErrorCode::OperationFailed;
    std::string message;
    int systemError = 0;
    std::string component;
    std::string operation;
};

} // namespace lx

