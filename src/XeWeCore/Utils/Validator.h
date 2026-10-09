// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/Validator.h
#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <type_traits>

#include "String.h"


namespace xewe {

template <typename>
struct always_false : std::false_type {};

// Use LimitT to allow implicit type deduction for literals (e.g. 0, 255)
template <typename T, typename LimitT>
std::optional<T> validate(std::string_view value, LimitT min, LimitT max) {
    using U = typename std::decay<T>::type;

    if constexpr (std::is_same_v<U, std::string>) {
        // For strings, min and max denote the string length
        const std::size_t len = value.length();
        if (len >= static_cast<std::size_t>(min) && len <= static_cast<std::size_t>(max)) {
            return std::string(value);
        }
    } else if constexpr (std::is_integral_v<U> && !std::is_same_v<U, bool>) {
        // For integers, min and max denote the numeric range bounds
        // parse as U itself, so a value outside U's range fails instead of being truncated
        // when the bounds are wider than the type (e.g. validate<int8_t>("300", 0, 1000))
        if constexpr (std::is_signed_v<U>) {
            U res = 0;
            if (xewe::str::parse_int(value, res) &&
                res >= static_cast<long long>(min) && res <= static_cast<long long>(max)) {
                return static_cast<T>(res);
            }
        } else if constexpr (std::is_unsigned_v<U>) {
            U res = 0;
            if (xewe::str::parse_int(value, res) &&
                res >= static_cast<unsigned long long>(min) && res <= static_cast<unsigned long long>(max)) {
                return static_cast<T>(res);
            }
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        // For floats, min and max denote the numeric range bounds
        double res = 0.0;
        if (xewe::str::parse_float(value, res) &&
            res >= static_cast<double>(min) && res <= static_cast<double>(max)) {
            return static_cast<T>(res);
        }
    } else {
        static_assert(always_false<U>::value, "Unsupported Validator::validate<T>() type.");
    }

    return std::nullopt;
}

} // namespace xewe
