#include "lx/cli/JsonSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <type_traits>

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

Json endpointValue(const Endpoint& endpoint)
{
    return {{"address", endpoint.address}, {"port", endpoint.port}};
}

Json socketValue(const SocketInfo& socket)
{
    Json remote = socket.remote ? endpointValue(*socket.remote) : Json(nullptr);
    return {{"protocol", socket.protocol == TransportProtocol::Tcp ? "tcp" : "udp"},
            {"family", socket.family == AddressFamily::IPv4 ? "ipv4" : "ipv6"},
            {"local", endpointValue(socket.local)},
            {"remote", std::move(remote)},
            {"state", socket.state},
            {"uid", socket.uid},
            {"inode", socket.inode},
            {"owner_pids", socket.ownerPids}};
}

Json portValue(const PortInfo& port)
{
    Json owners = Json::array();
    for (const auto& owner : port.owners) {
        owners.push_back(processValue(owner, false));
    }
    return {{"socket", socketValue(port.socket)}, {"owners", owners}};
}

Json optionalPid(const std::optional<pid_t>& value)
{
    return value ? Json(*value) : Json(nullptr);
}

Json serviceValue(const ServiceInfo& service)
{
    return {{"unit_name", service.unitName},
            {"description", service.description},
            {"load_state", service.loadState},
            {"active_state", service.activeState},
            {"sub_state", service.subState},
            {"unit_file_state", service.unitFileState},
            {"main_pid", optionalPid(service.mainPid)},
            {"active_enter_timestamp_usec", service.activeEnterTimestampUsec}};
}

Json journalValue(const JournalEntry& entry)
{
    const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        entry.timestamp.time_since_epoch()).count();
    return {{"timestamp_unix_usec", timestamp},
            {"cursor", entry.cursor},
            {"systemd_unit", optionalString(entry.systemdUnit)},
            {"pid", optionalPid(entry.pid)},
            {"command", optionalString(entry.command)},
            {"message", entry.message},
            {"priority", entry.priority ? Json(*entry.priority) : Json(nullptr)}};
}

Json portValues(const std::vector<PortInfo>& source)
{
    Json values = Json::array();
    for (const auto& value : source) values.push_back(portValue(value));
    return values;
}

Json processValues(const std::vector<ProcessInfo>& source)
{
    Json values = Json::array();
    for (const auto& value : source) values.push_back(processValue(value, false));
    return values;
}

Json serviceValues(const std::vector<ServiceInfo>& source)
{
    Json values = Json::array();
    for (const auto& value : source) values.push_back(serviceValue(value));
    return values;
}

Json journalValues(const std::vector<JournalEntry>& source)
{
    Json values = Json::array();
    for (const auto& value : source) values.push_back(journalValue(value));
    return values;
}

Json resourceTargetValue(const ResourceTarget& target)
{
    return std::visit([](const auto& value) -> Json {
        using Target = std::decay_t<decltype(value)>;
        if constexpr (std::is_same<Target, PortTarget>::value) {
            return {{"type", "port"}, {"value", value.port}};
        } else if constexpr (std::is_same<Target, ProcessTarget>::value) {
            return {{"type", "pid"}, {"value", value.pid}};
        } else {
            return {{"type", "service"}, {"value", value.unit}};
        }
    }, target);
}

const char* capabilityName(const CapabilityStatus status)
{
    switch (status) {
    case CapabilityStatus::Available: return "available";
    case CapabilityStatus::Unavailable: return "unavailable";
    case CapabilityStatus::NotImplemented: return "not_implemented";
    }
    return "not_implemented";
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

std::string JsonSerializer::ports(
    const Observation<std::vector<PortInfo>>& value)
{
    Json ports = Json::array();
    for (const auto& port : value.value) ports.push_back(portValue(port));
    return envelope("port", "list", {{"ports", ports}}, value.warnings).dump();
}

std::string JsonSerializer::service(const Observation<ServiceInfo>& value)
{
    return envelope("service", "inspect",
                    {{"service", serviceValue(value.value)}},
                    value.warnings).dump();
}

std::string JsonSerializer::services(
    const Observation<std::vector<ServiceInfo>>& value)
{
    Json services = Json::array();
    for (const auto& service : value.value) {
        services.push_back(serviceValue(service));
    }
    return envelope("service", "list", {{"services", services}},
                    value.warnings).dump();
}

std::string JsonSerializer::logs(
    const Observation<std::vector<JournalEntry>>& value)
{
    Json entries = Json::array();
    for (const auto& entry : value.value) entries.push_back(journalValue(entry));
    return envelope("log", "read", {{"entries", entries}},
                    value.warnings).dump();
}

std::string JsonSerializer::logEvent(const Observation<JournalEntry>& value)
{
    return envelope("log", "follow", {{"entry", journalValue(value.value)}},
                    value.warnings).dump();
}

std::string JsonSerializer::doctor(const DoctorReport& value)
{
    Json checks = Json::array();
    for (const auto& check : value.checks) {
        checks.push_back({{"name", check.name},
                          {"status", capabilityName(check.status)},
                          {"detail", check.detail}});
    }
    return envelope("doctor", "inspect", {{"checks", checks}}, {}).dump();
}

std::string JsonSerializer::inspect(const ResourceGraph& value)
{
    Json data{{"root", resourceTargetValue(value.root)},
              {"ports", portValues(value.ports)},
              {"processes", processValues(value.processes)},
              {"services", serviceValues(value.services)},
              {"recent_logs", journalValues(value.recentLogs)}};
    return envelope("inspect", "inspect", std::move(data),
                    value.warnings).dump();
}

std::string JsonSerializer::find(const FindResult& value)
{
    Json data{{"services", serviceValues(value.services)},
              {"processes", processValues(value.processes)},
              {"ports", portValues(value.ports)},
              {"executables", value.executables}};
    return envelope("find", "search", std::move(data), value.warnings).dump();
}

} // namespace lx::cli
