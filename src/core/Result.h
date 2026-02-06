#pragma once

#include <variant>
#include <string>
#include <stdexcept>

namespace mt {

struct Error {
    int code;
    std::string message;

    Error(int c, std::string msg) : code(c), message(std::move(msg)) {}
    explicit Error(std::string msg) : code(-1), message(std::move(msg)) {}
};

template<typename T>
class Result {
    std::variant<T, Error> value_;
public:
    Result(T val) : value_(std::move(val)) {}   // NOLINT: intentional implicit
    Result(Error err) : value_(std::move(err)) {} // NOLINT: intentional implicit

    bool ok() const { return std::holds_alternative<T>(value_); }
    explicit operator bool() const { return ok(); }

    const T& operator*() const {
        if (!ok()) throw std::runtime_error("Result access on error: " + error().message);
        return std::get<T>(value_);
    }
    T& operator*() {
        if (!ok()) throw std::runtime_error("Result access on error: " + error().message);
        return std::get<T>(value_);
    }
    const T* operator->() const { return &**this; }
    T* operator->() { return &**this; }

    const Error& error() const { return std::get<Error>(value_); }
};

template<>
class Result<void> {
    std::variant<std::monostate, Error> value_;
public:
    Result() : value_(std::monostate{}) {}
    Result(Error err) : value_(std::move(err)) {} // NOLINT: intentional implicit

    bool ok() const { return std::holds_alternative<std::monostate>(value_); }
    explicit operator bool() const { return ok(); }

    const Error& error() const { return std::get<Error>(value_); }

    static Result success() { return Result(); }
};

} // namespace mt
