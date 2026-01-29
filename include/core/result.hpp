#pragma once
#include "error.hpp"
#include <variant>

namespace ir {

    template <class T>
    class Result {
        std::variant<T, Error> v_;
    public:
        Result(T x) : v_(std::move(x)) {}
        Result(Error e) : v_(std::move(e)) {}

        bool has_value() const { return std::holds_alternative<T>(v_); }
        T& value() { return std::get<T>(v_); }
        const T& value() const { return std::get<T>(v_); }

        Error& error() { return std::get<Error>(v_); }
        const Error& error() const { return std::get<Error>(v_); }
    };

} // namespace ir