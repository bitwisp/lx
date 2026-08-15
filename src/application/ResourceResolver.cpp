#include "lx/application/ResourceResolver.h"

#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <charconv>
#include <limits>
#include <sstream>

namespace lx::application {
namespace {

template <typename Integer>
Result<Integer> positiveInteger(const std::string& text,
                                const Integer maximum,
                                const std::string& kind)
{
    unsigned long long value = 0;
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size() || value == 0 ||
        value > static_cast<unsigned long long>(maximum)) {
        return Result<Integer>::failure({
            ErrorCode::InvalidArgument, "Invalid " + kind + ": " + text, 0,
            "resource-resolver", "parse"});
    }
    return Result<Integer>::success(static_cast<Integer>(value));
}

Error notFound(const std::string& expression)
{
    return {ErrorCode::NotFound,
            "No observable resource matched \"" + expression + "\"", 0,
            "resource-resolver", "resolve"};
}

} // namespace

ResourceResolver::ResourceResolver(
    const PortService& portService, const ProcessService& processService,
    const ServiceService& serviceService) noexcept
    : portService_(portService), processService_(processService),
      serviceService_(serviceService)
{
}

Result<ResourceTarget> ResourceResolver::resolve(
    const std::string& expression) const
{
    if (expression.empty()) {
        return Result<ResourceTarget>::failure({
            ErrorCode::InvalidArgument, "Resource expression must not be empty",
            0, "resource-resolver", "resolve"});
    }

    const auto colon = expression.find(':');
    if (colon != std::string::npos) {
        const auto prefix = expression.substr(0, colon);
        const auto value = expression.substr(colon + 1);
        if (prefix == "port") {
            auto port = positiveInteger<std::uint16_t>(value, 65535, "port");
            if (!port) return Result<ResourceTarget>::failure(port.error());
            return Result<ResourceTarget>::success(PortTarget{port.value()});
        }
        if (prefix == "pid") {
            auto pid = positiveInteger<pid_t>(
                value, std::numeric_limits<pid_t>::max(), "PID");
            if (!pid) return Result<ResourceTarget>::failure(pid.error());
            return Result<ResourceTarget>::success(ProcessTarget{pid.value()});
        }
        if (prefix == "service") {
            auto unit = ServiceService::normalizeUnit(value);
            if (!unit) return Result<ResourceTarget>::failure(unit.error());
            return Result<ResourceTarget>::success(
                ServiceTarget{std::move(unit).value()});
        }
        return Result<ResourceTarget>::failure({
            ErrorCode::InvalidArgument,
            "Unknown resource prefix \"" + prefix +
                "\"; use port:, pid:, or service:",
            0, "resource-resolver", "resolve"});
    }

    const auto numeric = positiveInteger<unsigned long long>(
        expression, std::numeric_limits<unsigned long long>::max(), "number");
    if (numeric) {
        bool processExists = false;
        bool portExists = false;
        if (numeric.value() <=
            static_cast<unsigned long long>(std::numeric_limits<pid_t>::max())) {
            processExists = static_cast<bool>(
                processService_.inspect(static_cast<pid_t>(numeric.value())));
        }
        if (numeric.value() <= 65535) {
            SocketQuery query;
            query.localPort = static_cast<std::uint16_t>(numeric.value());
            const auto ports = portService_.inspect(query);
            portExists = ports && !ports.value().value.empty();
        }
        if (processExists && portExists) {
            return Result<ResourceTarget>::failure({
                ErrorCode::Conflict,
                "Multiple resources matched \"" + expression +
                    "\": port:" + expression + " and pid:" + expression,
                0, "resource-resolver", "resolve"});
        }
        if (portExists) {
            return Result<ResourceTarget>::success(
                PortTarget{static_cast<std::uint16_t>(numeric.value())});
        }
        if (processExists) {
            return Result<ResourceTarget>::success(
                ProcessTarget{static_cast<pid_t>(numeric.value())});
        }
        return Result<ResourceTarget>::failure(notFound(expression));
    }

    auto service = serviceService_.inspect(expression);
    if (service) {
        return Result<ResourceTarget>::success(
            ServiceTarget{service.value().value.unitName});
    }

    ProcessQuery query;
    query.name = expression;
    auto processes = processService_.list(std::move(query));
    if (!processes) return Result<ResourceTarget>::failure(processes.error());
    if (processes.value().value.size() == 1) {
        return Result<ResourceTarget>::success(
            ProcessTarget{processes.value().value.front().pid});
    }
    if (processes.value().value.size() > 1) {
        std::ostringstream message;
        message << "Multiple processes matched \"" << expression
                << "\"; use";
        for (const auto& process : processes.value().value) {
            message << " pid:" << process.pid;
        }
        return Result<ResourceTarget>::failure({
            ErrorCode::Conflict, message.str(), 0, "resource-resolver",
            "resolve"});
    }
    return Result<ResourceTarget>::failure(notFound(expression));
}

} // namespace lx::application
