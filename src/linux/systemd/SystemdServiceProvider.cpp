#include "lx/linux/systemd/SystemdServiceProvider.h"

#include "lx/linux/systemd/SdBus.h"
#include "lx/linux/systemd/SystemdMessageParser.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <systemd/sd-bus.h>

namespace lx::linux {
namespace {

constexpr const char* destination = "org.freedesktop.systemd1";
constexpr const char* managerPath = "/org/freedesktop/systemd1";
constexpr const char* managerInterface = "org.freedesktop.systemd1.Manager";
constexpr const char* unitInterface = "org.freedesktop.systemd1.Unit";
constexpr const char* serviceInterface = "org.freedesktop.systemd1.Service";

Error busError(const sd_bus_error& error, const int status,
               const std::string& operation)
{
    const int number = status < 0 ? -status : status;
    ErrorCode code = ErrorCode::OperationFailed;
    if (number == EACCES || number == EPERM ||
        sd_bus_error_has_name(&error, SD_BUS_ERROR_ACCESS_DENIED)) {
        code = ErrorCode::PermissionDenied;
    } else if (number == ETIMEDOUT ||
               sd_bus_error_has_name(&error, SD_BUS_ERROR_NO_REPLY)) {
        code = ErrorCode::Timeout;
    } else if (number == ENOENT ||
               sd_bus_error_has_name(
                   &error, "org.freedesktop.systemd1.NoSuchUnit") ||
               sd_bus_error_has_name(
                   &error, "org.freedesktop.systemd1.NoUnitForPID")) {
        code = ErrorCode::NotFound;
    } else if (number == ECONNREFUSED || number == ENOTCONN ||
               sd_bus_error_has_name(&error, SD_BUS_ERROR_SERVICE_UNKNOWN) ||
               sd_bus_error_has_name(&error, SD_BUS_ERROR_NAME_HAS_NO_OWNER)) {
        code = ErrorCode::Unavailable;
    }

    std::string message;
    if (error.message != nullptr) {
        message = error.message;
    } else if (number != 0) {
        message = std::strerror(number);
    } else {
        message = "systemd D-Bus operation failed";
    }
    return {code, std::move(message), number, "systemd", operation};
}

Result<std::string> getUnitPath(sd_bus* bus, const std::string& unit)
{
    SdBusError error;
    SdBusMessage reply;
    const int status = sd_bus_call_method(
        bus, destination, managerPath, managerInterface, "GetUnit", error.get(),
        reply.put(), "s", unit.c_str());
    if (status < 0) {
        return Result<std::string>::failure(
            busError(error.value(), status, "get unit"));
    }
    const char* path = nullptr;
    if (sd_bus_message_read(reply.get(), "o", &path) < 0 || path == nullptr) {
        return Result<std::string>::failure(
            {ErrorCode::ProtocolError, "GetUnit returned no object path", 0,
             "systemd", "get unit"});
    }
    return Result<std::string>::success(path);
}

Result<std::string> stringProperty(sd_bus* bus, const std::string& path,
                                   const char* interface, const char* property)
{
    SdBusError error;
    char* rawValue = nullptr;
    const int status = sd_bus_get_property_string(
        bus, destination, path.c_str(), interface, property, error.get(),
        &rawValue);
    std::unique_ptr<char, decltype(&std::free)> value{rawValue, &std::free};
    if (status < 0) {
        return Result<std::string>::failure(
            busError(error.value(), status, "read service property"));
    }
    return Result<std::string>::success(value == nullptr ? "" : value.get());
}

template <typename T>
Result<T> trivialProperty(sd_bus* bus, const std::string& path,
                          const char* interface, const char* property,
                          const char type)
{
    T value{};
    SdBusError error;
    const int status = sd_bus_get_property_trivial(
        bus, destination, path.c_str(), interface, property, error.get(), type,
        &value);
    if (status < 0) {
        return Result<T>::failure(
            busError(error.value(), status, "read service property"));
    }
    return Result<T>::success(value);
}

Result<std::string> unitFileState(sd_bus* bus, const std::string& unit)
{
    SdBusError error;
    SdBusMessage reply;
    const int status = sd_bus_call_method(
        bus, destination, managerPath, managerInterface, "GetUnitFileState",
        error.get(), reply.put(), "s", unit.c_str());
    if (status < 0) {
        return Result<std::string>::failure(
            busError(error.value(), status, "get unit file state"));
    }
    const char* state = nullptr;
    if (sd_bus_message_read(reply.get(), "s", &state) < 0 || state == nullptr) {
        return Result<std::string>::failure(
            {ErrorCode::ProtocolError, "GetUnitFileState returned no state", 0,
             "systemd", "get unit file state"});
    }
    return Result<std::string>::success(state);
}

Warning warningFrom(const Error& error)
{
    return {error.code, error.message, error.systemError, error.component,
            error.operation};
}

Result<void> controlUnit(const std::string& unit, const char* method,
                         const std::string& operation)
{
    auto connection = SdBusConnection::openSystem();
    if (!connection) return Result<void>::failure(connection.error());

    SdBusError error;
    SdBusMessage reply;
    const int status = sd_bus_call_method(
        connection.value().get(), destination, managerPath, managerInterface,
        method, error.get(), reply.put(), "ss", unit.c_str(), "replace");
    if (status < 0) {
        return Result<void>::failure(busError(error.value(), status, operation));
    }

    const char* jobPath = nullptr;
    if (sd_bus_message_read(reply.get(), "o", &jobPath) < 0 ||
        jobPath == nullptr) {
        return Result<void>::failure(
            {ErrorCode::ProtocolError,
             "systemd returned no job object path", 0, "systemd", operation});
    }
    return Result<void>::success();
}

} // namespace

Result<void> SystemdServiceProvider::probe() const
{
    auto connection = SdBusConnection::openSystem();
    if (!connection) return Result<void>::failure(connection.error());

    SdBusError error;
    SdBusMessage reply;
    const int status = sd_bus_get_property(
        connection.value().get(), destination, managerPath, managerInterface,
        "Version", error.get(), reply.put(), "s");
    if (status < 0) {
        return Result<void>::failure(busError(error.value(), status, "probe"));
    }
    return Result<void>::success();
}

Result<Observation<std::vector<ServiceInfo>>>
SystemdServiceProvider::list() const
{
    auto connection = SdBusConnection::openSystem();
    if (!connection) {
        return Result<Observation<std::vector<ServiceInfo>>>::failure(
            connection.error());
    }

    SdBusError error;
    SdBusMessage reply;
    const int status = sd_bus_call_method(
        connection.value().get(), destination, managerPath, managerInterface,
        "ListUnits", error.get(), reply.put(), "");
    if (status < 0) {
        return Result<Observation<std::vector<ServiceInfo>>>::failure(
            busError(error.value(), status, "list services"));
    }

    auto parsed = parseListUnitsMessage(reply.get());
    if (!parsed) {
        return Result<Observation<std::vector<ServiceInfo>>>::failure(
            parsed.error());
    }
    Observation<std::vector<ServiceInfo>> observation{
        std::move(parsed).value(), {}};
    for (auto& service : observation.value) {
        auto fileState = unitFileState(connection.value().get(),
                                       service.unitName);
        if (fileState) {
            service.unitFileState = std::move(fileState).value();
        } else {
            observation.warnings.push_back(warningFrom(fileState.error()));
        }

        auto path = getUnitPath(connection.value().get(), service.unitName);
        if (!path) {
            observation.warnings.push_back(warningFrom(path.error()));
            continue;
        }
        auto mainPid = trivialProperty<std::uint32_t>(
            connection.value().get(), path.value(), serviceInterface,
            "MainPID", SD_BUS_TYPE_UINT32);
        if (mainPid && mainPid.value() != 0) {
            service.mainPid = static_cast<pid_t>(mainPid.value());
        } else if (!mainPid) {
            observation.warnings.push_back(warningFrom(mainPid.error()));
        }
    }
    return Result<Observation<std::vector<ServiceInfo>>>::success(
        std::move(observation));
}

Result<Observation<ServiceInfo>> SystemdServiceProvider::get(
    const std::string& unit) const
{
    auto connection = SdBusConnection::openSystem();
    if (!connection) {
        return Result<Observation<ServiceInfo>>::failure(connection.error());
    }
    sd_bus* bus = connection.value().get();
    auto path = getUnitPath(bus, unit);
    if (!path) return Result<Observation<ServiceInfo>>::failure(path.error());

    auto description = stringProperty(bus, path.value(), unitInterface,
                                      "Description");
    auto loadState = stringProperty(bus, path.value(), unitInterface,
                                    "LoadState");
    auto activeState = stringProperty(bus, path.value(), unitInterface,
                                      "ActiveState");
    auto subState = stringProperty(bus, path.value(), unitInterface, "SubState");
    if (!description) {
        return Result<Observation<ServiceInfo>>::failure(description.error());
    }
    if (!loadState) {
        return Result<Observation<ServiceInfo>>::failure(loadState.error());
    }
    if (!activeState) {
        return Result<Observation<ServiceInfo>>::failure(activeState.error());
    }
    if (!subState) {
        return Result<Observation<ServiceInfo>>::failure(subState.error());
    }

    Observation<ServiceInfo> observation;
    observation.value.unitName = unit;
    observation.value.description = std::move(description).value();
    observation.value.loadState = std::move(loadState).value();
    observation.value.activeState = std::move(activeState).value();
    observation.value.subState = std::move(subState).value();

    auto fileState = unitFileState(bus, unit);
    if (fileState) {
        observation.value.unitFileState = std::move(fileState).value();
    } else {
        observation.warnings.push_back(warningFrom(fileState.error()));
    }

    auto mainPid = trivialProperty<std::uint32_t>(
        bus, path.value(), serviceInterface, "MainPID", SD_BUS_TYPE_UINT32);
    if (mainPid) {
        if (mainPid.value() != 0) {
            observation.value.mainPid = static_cast<pid_t>(mainPid.value());
        }
    } else {
        observation.warnings.push_back(warningFrom(mainPid.error()));
    }

    auto timestamp = trivialProperty<std::uint64_t>(
        bus, path.value(), unitInterface, "ActiveEnterTimestamp",
        SD_BUS_TYPE_UINT64);
    if (timestamp) {
        observation.value.activeEnterTimestampUsec = timestamp.value();
    } else {
        observation.warnings.push_back(warningFrom(timestamp.error()));
    }

    return Result<Observation<ServiceInfo>>::success(std::move(observation));
}

Result<std::optional<std::string>>
SystemdServiceProvider::unitByPid(const pid_t pid) const
{
    if (pid <= 0) {
        return Result<std::optional<std::string>>::failure(
            {ErrorCode::InvalidArgument, "PID must be greater than zero", 0,
             "systemd", "resolve PID"});
    }

    auto connection = SdBusConnection::openSystem();
    if (!connection) {
        return Result<std::optional<std::string>>::failure(connection.error());
    }

    SdBusError error;
    SdBusMessage reply;
    const auto systemdPid = static_cast<std::uint32_t>(pid);
    const int status = sd_bus_call_method(
        connection.value().get(), destination, managerPath, managerInterface,
        "GetUnitByPID", error.get(), reply.put(), "u", systemdPid);
    if (status < 0) {
        auto mapped = busError(error.value(), status, "resolve PID");
        if (mapped.code == ErrorCode::NotFound) {
            return Result<std::optional<std::string>>::success(std::nullopt);
        }
        return Result<std::optional<std::string>>::failure(std::move(mapped));
    }

    const char* rawPath = nullptr;
    if (sd_bus_message_read(reply.get(), "o", &rawPath) < 0 ||
        rawPath == nullptr) {
        return Result<std::optional<std::string>>::failure(
            {ErrorCode::ProtocolError,
             "GetUnitByPID returned no object path", 0, "systemd",
             "resolve PID"});
    }
    const std::string path{rawPath};
    auto id = stringProperty(connection.value().get(), path, unitInterface,
                             "Id");
    if (!id) {
        if (id.error().code == ErrorCode::NotFound) {
            return Result<std::optional<std::string>>::success(std::nullopt);
        }
        return Result<std::optional<std::string>>::failure(id.error());
    }

    constexpr std::string_view suffix = ".service";
    if (id.value().size() <= suffix.size() ||
        std::string_view{id.value()}.substr(id.value().size() - suffix.size()) !=
            suffix) {
        return Result<std::optional<std::string>>::success(std::nullopt);
    }
    return Result<std::optional<std::string>>::success(
        std::optional<std::string>{std::move(id).value()});
}

Result<void> SystemdServiceProvider::start(const std::string& unit) const
{
    return controlUnit(unit, "StartUnit", "start service");
}

Result<void> SystemdServiceProvider::stop(const std::string& unit) const
{
    return controlUnit(unit, "StopUnit", "stop service");
}

Result<void> SystemdServiceProvider::restart(const std::string& unit) const
{
    return controlUnit(unit, "RestartUnit", "restart service");
}

} // namespace lx::linux
