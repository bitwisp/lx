#pragma once

#include "lx/domain/Result.h"

#include <systemd/sd-bus.h>

namespace lx::linux {

class SdBusConnection final {
public:
    SdBusConnection() noexcept = default;
    ~SdBusConnection();

    SdBusConnection(const SdBusConnection&) = delete;
    SdBusConnection& operator=(const SdBusConnection&) = delete;
    SdBusConnection(SdBusConnection&& other) noexcept;
    SdBusConnection& operator=(SdBusConnection&& other) noexcept;

    [[nodiscard]] static Result<SdBusConnection> openSystem();
    [[nodiscard]] sd_bus* get() const noexcept;

private:
    explicit SdBusConnection(sd_bus* bus) noexcept;

    sd_bus* bus_ = nullptr;
};

class SdBusMessage final {
public:
    SdBusMessage() noexcept = default;
    explicit SdBusMessage(sd_bus_message* message) noexcept;
    ~SdBusMessage();

    SdBusMessage(const SdBusMessage&) = delete;
    SdBusMessage& operator=(const SdBusMessage&) = delete;
    SdBusMessage(SdBusMessage&& other) noexcept;
    SdBusMessage& operator=(SdBusMessage&& other) noexcept;

    [[nodiscard]] sd_bus_message* get() const noexcept;
    [[nodiscard]] sd_bus_message** put() noexcept;

private:
    sd_bus_message* message_ = nullptr;
};

class SdBusError final {
public:
    SdBusError() noexcept = default;
    ~SdBusError();

    SdBusError(const SdBusError&) = delete;
    SdBusError& operator=(const SdBusError&) = delete;
    SdBusError(SdBusError&&) = delete;
    SdBusError& operator=(SdBusError&&) = delete;

    [[nodiscard]] sd_bus_error* get() noexcept;
    [[nodiscard]] const sd_bus_error& value() const noexcept;

private:
    sd_bus_error error_ = SD_BUS_ERROR_NULL;
};

} // namespace lx::linux
