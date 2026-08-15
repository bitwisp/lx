#include "lx/linux/systemd/SystemdServiceProvider.h"

#include "lx/linux/systemd/SdBus.h"
#include "lx/linux/systemd/SystemdMessageParser.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <systemd/sd-bus.h>

namespace lx::linux {
namespace {

constexpr const char* destination = "org.freedesktop.systemd1";
constexpr const char* managerPath = "/org/freedesktop/systemd1";
constexpr const char* managerInterface = "org.freedesktop.systemd1.Manager";

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
                   &error, "org.freedesktop.systemd1.NoSuchUnit")) {
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

template <typename T>
Result<T> unsupported(const std::string& operation)
{
    return Result<T>::failure({ErrorCode::Unsupported,
                               "systemd operation is not implemented yet", 0,
                               "systemd", operation});
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
    return Result<Observation<std::vector<ServiceInfo>>>::success(
        {std::move(parsed).value(), {}});
}

Result<Observation<ServiceInfo>> SystemdServiceProvider::get(
    const std::string&) const
{
    return unsupported<Observation<ServiceInfo>>("get service");
}

Result<std::optional<std::string>>
SystemdServiceProvider::unitByPid(const pid_t) const
{
    return unsupported<std::optional<std::string>>("resolve PID");
}

Result<void> SystemdServiceProvider::start(const std::string&) const
{
    return unsupported<void>("start service");
}

Result<void> SystemdServiceProvider::stop(const std::string&) const
{
    return unsupported<void>("stop service");
}

Result<void> SystemdServiceProvider::restart(const std::string&) const
{
    return unsupported<void>("restart service");
}

} // namespace lx::linux
