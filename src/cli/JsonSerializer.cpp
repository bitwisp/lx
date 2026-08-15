#include "lx/cli/JsonSerializer.h"

#include <nlohmann/json.hpp>

namespace lx::cli {
namespace {
using Json = nlohmann::json;

std::string errorCodeName(const ErrorCode code)
{
    switch (code) {
    case ErrorCode::InvalidArgument: return "invalid_argument";
    case ErrorCode::NotFound: return "not_found";
    case ErrorCode::PermissionDenied: return "permission_denied";
    case ErrorCode::Unsupported: return "unsupported";
    case ErrorCode::Unavailable: return "unavailable";
    case ErrorCode::IoError: return "io_error";
    case ErrorCode::ProtocolError: return "protocol_error";
    case ErrorCode::ParseError: return "parse_error";
    case ErrorCode::OperationFailed: return "operation_failed";
    case ErrorCode::Timeout: return "timeout";
    case ErrorCode::Conflict: return "conflict";
    case ErrorCode::Interrupted: return "interrupted";
    }
    return "operation_failed";
}

Json errorValue(const Error& error)
{
    return {{"code", errorCodeName(error.code)},
            {"message", error.message},
            {"system_error", error.systemError},
            {"component", error.component},
            {"operation", error.operation}};
}

Json warningValue(const Warning& warning)
{
    return {{"code", errorCodeName(warning.code)},
            {"message", warning.message},
            {"system_error", warning.systemError},
            {"component", warning.component},
            {"operation", warning.operation}};
}

Json warningValues(const std::vector<Warning>& warnings)
{
    Json values = Json::array();
    for (const auto& warning : warnings) values.push_back(warningValue(warning));
    return values;
}
} // namespace

std::string JsonSerializer::error(
    const std::string& command, const std::string& operation,
    const Error& value)
{
    return Json{{"schema_version", schemaVersion},
                {"command", command},
                {"operation", operation},
                {"error", errorValue(value)}}.dump();
}

std::string JsonSerializer::emptySuccess(
    const std::string& command, const std::string& operation,
    const std::vector<Warning>& warnings)
{
    return Json{{"schema_version", schemaVersion},
                {"command", command},
                {"operation", operation},
                {"data", Json::object()},
                {"warnings", warningValues(warnings)}}.dump();
}

} // namespace lx::cli
