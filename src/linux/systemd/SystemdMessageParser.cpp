#include "lx/linux/systemd/SystemdMessageParser.h"

#include <string>
#include <string_view>

namespace lx::linux {
namespace {

Result<std::vector<ServiceInfo>> parseFailure(const std::string& message)
{
    return Result<std::vector<ServiceInfo>>::failure(
        {ErrorCode::ProtocolError, message, 0, "systemd", "parse ListUnits"});
}

bool isService(const std::string_view value)
{
    constexpr std::string_view suffix = ".service";
    return value.size() > suffix.size() &&
           value.substr(value.size() - suffix.size()) == suffix;
}

} // namespace

std::optional<ServiceInfo> serviceFromUnitRecord(
    const SystemdUnitRecord& record)
{
    if (!isService(record.name)) return std::nullopt;

    ServiceInfo service;
    service.unitName = record.name;
    service.description = record.description;
    service.loadState = record.loadState;
    service.activeState = record.activeState;
    service.subState = record.subState;
    return service;
}

Result<std::vector<ServiceInfo>> parseListUnitsMessage(sd_bus_message* message)
{
    if (message == nullptr) {
        return parseFailure("ListUnits returned no message");
    }

    int status = sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY,
                                                "(ssssssouso)");
    if (status < 0) {
        return parseFailure("ListUnits response is not an array of units");
    }

    std::vector<ServiceInfo> services;
    while ((status = sd_bus_message_enter_container(
                message, SD_BUS_TYPE_STRUCT, "ssssssouso")) > 0) {
        const char* name = nullptr;
        const char* description = nullptr;
        const char* loadState = nullptr;
        const char* activeState = nullptr;
        const char* subState = nullptr;
        const char* following = nullptr;
        const char* objectPath = nullptr;
        std::uint32_t jobId = 0;
        const char* jobType = nullptr;
        const char* jobPath = nullptr;

        status = sd_bus_message_read(message, "ssssssouso", &name,
                                     &description, &loadState, &activeState,
                                     &subState, &following, &objectPath, &jobId,
                                     &jobType, &jobPath);
        if (status < 0) {
            return parseFailure("Unable to decode a ListUnits record");
        }
        status = sd_bus_message_exit_container(message);
        if (status < 0) {
            return parseFailure("Malformed ListUnits unit record");
        }

        auto service = serviceFromUnitRecord(
            {name == nullptr ? "" : name,
             description == nullptr ? "" : description,
             loadState == nullptr ? "" : loadState,
             activeState == nullptr ? "" : activeState,
             subState == nullptr ? "" : subState});
        if (service) {
            services.push_back(std::move(*service));
        }
    }
    if (status < 0 || sd_bus_message_exit_container(message) < 0) {
        return parseFailure("Malformed ListUnits response");
    }
    return Result<std::vector<ServiceInfo>>::success(std::move(services));
}

} // namespace lx::linux
