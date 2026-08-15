#include "lx/linux/systemd/SdBus.h"

#include <cerrno>
#include <string>
#include <system_error>
#include <utility>

namespace lx::linux {
namespace {

Error openError(const int status)
{
    const int number = status < 0 ? -status : status;
    ErrorCode code = ErrorCode::Unavailable;
    if (number == EACCES || number == EPERM) {
        code = ErrorCode::PermissionDenied;
    }
    return {code,
            "Unable to open the system bus: " +
                std::system_category().message(number),
            number,
            "sd-bus",
            "open system bus"};
}

} // namespace

SdBusConnection::SdBusConnection(sd_bus* bus) noexcept : bus_(bus) {}

SdBusConnection::~SdBusConnection()
{
    sd_bus_unref(bus_);
}

SdBusConnection::SdBusConnection(SdBusConnection&& other) noexcept
    : bus_(std::exchange(other.bus_, nullptr))
{
}

SdBusConnection& SdBusConnection::operator=(SdBusConnection&& other) noexcept
{
    if (this != &other) {
        sd_bus_unref(bus_);
        bus_ = std::exchange(other.bus_, nullptr);
    }
    return *this;
}

Result<SdBusConnection> SdBusConnection::openSystem()
{
    sd_bus* bus = nullptr;
    const int status = sd_bus_open_system(&bus);
    if (status < 0) {
        return Result<SdBusConnection>::failure(openError(status));
    }
    return Result<SdBusConnection>::success(SdBusConnection{bus});
}

sd_bus* SdBusConnection::get() const noexcept
{
    return bus_;
}

SdBusMessage::SdBusMessage(sd_bus_message* message) noexcept
    : message_(message)
{
}

SdBusMessage::~SdBusMessage()
{
    sd_bus_message_unref(message_);
}

SdBusMessage::SdBusMessage(SdBusMessage&& other) noexcept
    : message_(std::exchange(other.message_, nullptr))
{
}

SdBusMessage& SdBusMessage::operator=(SdBusMessage&& other) noexcept
{
    if (this != &other) {
        sd_bus_message_unref(message_);
        message_ = std::exchange(other.message_, nullptr);
    }
    return *this;
}

sd_bus_message* SdBusMessage::get() const noexcept
{
    return message_;
}

sd_bus_message** SdBusMessage::put() noexcept
{
    sd_bus_message_unref(message_);
    message_ = nullptr;
    return &message_;
}

SdBusError::~SdBusError()
{
    sd_bus_error_free(&error_);
}

sd_bus_error* SdBusError::get() noexcept
{
    return &error_;
}

const sd_bus_error& SdBusError::value() const noexcept
{
    return error_;
}

} // namespace lx::linux
