#include "lx/linux/systemd/UnavailableServiceProvider.h"

#include <utility>

namespace lx::linux {

UnavailableServiceProvider::UnavailableServiceProvider(std::string reason)
    : reason_(std::move(reason))
{
}

Error UnavailableServiceProvider::unavailable(
    const std::string& operation) const
{
    return {ErrorCode::Unavailable, reason_, 0, "systemd", operation};
}

Result<void> UnavailableServiceProvider::probe() const
{
    return Result<void>::failure(unavailable("probe"));
}

Result<Observation<std::vector<ServiceInfo>>>
UnavailableServiceProvider::list() const
{
    return Result<Observation<std::vector<ServiceInfo>>>::failure(
        unavailable("list services"));
}

Result<Observation<ServiceInfo>> UnavailableServiceProvider::get(
    const std::string&) const
{
    return Result<Observation<ServiceInfo>>::failure(
        unavailable("get service"));
}

Result<std::optional<std::string>>
UnavailableServiceProvider::unitByPid(const pid_t) const
{
    return Result<std::optional<std::string>>::failure(
        unavailable("resolve PID"));
}

Result<void> UnavailableServiceProvider::start(const std::string&) const
{
    return Result<void>::failure(unavailable("start service"));
}

Result<void> UnavailableServiceProvider::stop(const std::string&) const
{
    return Result<void>::failure(unavailable("stop service"));
}

Result<void> UnavailableServiceProvider::restart(const std::string&) const
{
    return Result<void>::failure(unavailable("restart service"));
}

} // namespace lx::linux
