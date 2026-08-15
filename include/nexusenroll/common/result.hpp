#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace nexusenroll::common {

struct Error {
    std::string code;
    std::string message;
};

template <typename T>
class Result {
public:
    static Result success(T value) {
        return Result(std::move(value), std::nullopt);
    }

    static Result failure(std::string code, std::string message) {
        return Result(std::nullopt, Error{std::move(code), std::move(message)});
    }

    bool hasValue() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return hasValue(); }

    const T& value() const {
        if (!value_) {
            throw std::logic_error("Attempted to access a failed result");
        }
        return *value_;
    }

    T& value() {
        if (!value_) {
            throw std::logic_error("Attempted to access a failed result");
        }
        return *value_;
    }

    const Error& error() const {
        if (!error_) {
            throw std::logic_error("Attempted to access an error on a successful result");
        }
        return *error_;
    }

private:
    Result(std::optional<T> value, std::optional<Error> error)
        : value_(std::move(value)), error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<Error> error_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(std::nullopt); }

    static Result failure(std::string code, std::string message) {
        return Result(Error{std::move(code), std::move(message)});
    }

    bool hasValue() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return hasValue(); }

    const Error& error() const {
        if (!error_) {
            throw std::logic_error("Attempted to access an error on a successful result");
        }
        return *error_;
    }

private:
    explicit Result(std::optional<Error> error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

}
