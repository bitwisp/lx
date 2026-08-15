#include "lx/cli/JsonSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

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

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool secretKey(const std::string& value)
{
    auto key = lower(value);
    return key == "password" || key == "passwd" || key == "token" ||
           key == "api-key" || key == "apikey" || key == "secret" ||
           key == "database_url";
}

std::vector<std::string> arguments(
    const std::vector<std::string>& source, const bool raw)
{
    if (raw) return source;
    auto values = source;
    bool redactNext = false;
    for (auto& argument : values) {
        if (redactNext) {
            argument = "<redacted>";
            redactNext = false;
            continue;
        }
        const auto equal = argument.find('=');
        auto key = equal == std::string::npos
                       ? argument
                       : argument.substr(0, equal);
        while (!key.empty() && key.front() == '-') key.erase(key.begin());
        if (secretKey(key)) {
            if (equal == std::string::npos) redactNext = true;
            else argument = argument.substr(0, equal + 1) + "<redacted>";
        }
    }
    return values;
}

Json optionalString(const std::optional<std::string>& value)
{
    return value ? Json(*value) : Json(nullptr);
}

Json processValue(const ProcessInfo& process, const bool raw)
{
    return {{"pid", process.pid},
            {"ppid", process.ppid},
            {"name", process.name},
            {"state", process.state},
            {"uid", process.uid},
            {"gid", process.gid},
            {"user", process.user},
            {"executable", optionalString(process.executable)},
            {"cwd", optionalString(process.cwd)},
            {"argv", arguments(process.argv, raw)},
            {"rss_bytes", process.rssBytes},
            {"threads", process.threads},
            {"systemd_unit", optionalString(process.systemdUnit)}};
}

Json envelope(const std::string& command, const std::string& operation,
              Json data, const std::vector<Warning>& warnings)
{
    return {{"schema_version", JsonSerializer::schemaVersion},
            {"command", command},
            {"operation", operation},
            {"data", std::move(data)},
            {"warnings", warningValues(warnings)}};
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

std::string JsonSerializer::process(
    const Observation<ProcessInfo>& value, const bool rawCommand)
{
    return envelope("process", "inspect",
                    {{"process", processValue(value.value, rawCommand)}},
                    value.warnings).dump();
}

std::string JsonSerializer::processes(
    const Observation<std::vector<ProcessInfo>>& value)
{
    Json processes = Json::array();
    for (const auto& process : value.value) {
        processes.push_back(processValue(process, false));
    }
    return envelope("process", "list", {{"processes", processes}},
                    value.warnings).dump();
}

} // namespace lx::cli
