#include "lx/application/ServiceService.h"

#include <cctype>
#include <string_view>

namespace lx::application {
namespace {

bool validUnitCharacter(const unsigned char character)
{
    return std::isalnum(character) != 0 || character == ':' ||
           character == '_' || character == '-' || character == '.' ||
           character == '@' || character == '\\';
}

Result<std::string> invalidUnit(const std::string& message)
{
    return Result<std::string>::failure(
        {ErrorCode::InvalidArgument, message, 0, "service", "validate unit"});
}

} // namespace

ServiceService::ServiceService(
    const contracts::IServiceProvider& serviceProvider) noexcept
    : serviceProvider_(serviceProvider)
{
}

Result<std::string> ServiceService::normalizeUnit(const std::string& unit)
{
    if (unit.empty()) return invalidUnit("Service unit must not be empty");
    if (unit.size() > 247) {
        return invalidUnit("Service unit name is too long");
    }
    for (const unsigned char character : unit) {
        if (!validUnitCharacter(character)) {
            return invalidUnit("Service unit contains an invalid character");
        }
    }

    constexpr std::string_view suffix = ".service";
    if (unit.size() >= suffix.size() &&
        std::string_view{unit}.substr(unit.size() - suffix.size()) == suffix) {
        return Result<std::string>::success(unit);
    }
    return Result<std::string>::success(unit + std::string{suffix});
}

Result<Observation<std::vector<ServiceInfo>>> ServiceService::list() const
{
    return serviceProvider_.list();
}

Result<Observation<ServiceInfo>> ServiceService::inspect(
    const std::string& unit) const
{
    auto normalized = normalizeUnit(unit);
    if (!normalized) {
        return Result<Observation<ServiceInfo>>::failure(normalized.error());
    }
    return serviceProvider_.get(normalized.value());
}

Result<std::optional<std::string>> ServiceService::unitByPid(
    const pid_t pid) const
{
    if (pid <= 0) {
        return Result<std::optional<std::string>>::failure(
            {ErrorCode::InvalidArgument, "PID must be greater than zero", 0,
             "service", "resolve PID"});
    }
    return serviceProvider_.unitByPid(pid);
}

Result<void> ServiceService::start(const std::string& unit) const
{
    auto normalized = normalizeUnit(unit);
    if (!normalized) return Result<void>::failure(normalized.error());
    return serviceProvider_.start(normalized.value());
}

Result<void> ServiceService::stop(const std::string& unit) const
{
    auto normalized = normalizeUnit(unit);
    if (!normalized) return Result<void>::failure(normalized.error());
    return serviceProvider_.stop(normalized.value());
}

Result<void> ServiceService::restart(const std::string& unit) const
{
    auto normalized = normalizeUnit(unit);
    if (!normalized) return Result<void>::failure(normalized.error());
    return serviceProvider_.restart(normalized.value());
}

} // namespace lx::application
