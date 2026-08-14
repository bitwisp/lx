#pragma once

#include "lx/domain/Error.h"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace lx {

template <typename T>
class Result final {
    static_assert(!std::is_void<T>::value, "Use Result<void> for void results");

public:
    static Result success(T value)
    {
        return Result(std::in_place_index<0>, std::move(value));
    }

    static Result failure(Error error)
    {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept
    {
        return storage_.index() == 0;
    }

    explicit operator bool() const noexcept
    {
        return hasValue();
    }

    T& value() &
    {
        requireValue();
        return std::get<0>(storage_);
    }

    const T& value() const&
    {
        requireValue();
        return std::get<0>(storage_);
    }

    T&& value() &&
    {
        requireValue();
        return std::get<0>(std::move(storage_));
    }

    Error& error() &
    {
        requireError();
        return std::get<1>(storage_);
    }

    const Error& error() const&
    {
        requireError();
        return std::get<1>(storage_);
    }

private:
    Result(std::in_place_index_t<0>, T value)
        : storage_(std::in_place_index<0>, std::move(value))
    {
    }

    Result(std::in_place_index_t<1>, Error error)
        : storage_(std::in_place_index<1>, std::move(error))
    {
    }

    void requireValue() const
    {
        if (!hasValue()) {
            throw std::logic_error("Result does not contain a value");
        }
    }

    void requireError() const
    {
        if (hasValue()) {
            throw std::logic_error("Result does not contain an error");
        }
    }

    std::variant<T, Error> storage_;
};

template <>
class Result<void> final {
public:
    static Result success()
    {
        return Result(std::in_place_index<0>);
    }

    static Result failure(Error error)
    {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept
    {
        return storage_.index() == 0;
    }

    explicit operator bool() const noexcept
    {
        return hasValue();
    }

    void value() const
    {
        if (!hasValue()) {
            throw std::logic_error("Result does not contain a value");
        }
    }

    Error& error() &
    {
        requireError();
        return std::get<1>(storage_);
    }

    const Error& error() const&
    {
        requireError();
        return std::get<1>(storage_);
    }

private:
    explicit Result(std::in_place_index_t<0>)
        : storage_(std::in_place_index<0>)
    {
    }

    Result(std::in_place_index_t<1>, Error error)
        : storage_(std::in_place_index<1>, std::move(error))
    {
    }

    void requireError() const
    {
        if (hasValue()) {
            throw std::logic_error("Result does not contain an error");
        }
    }

    std::variant<std::monostate, Error> storage_;
};

} // namespace lx

