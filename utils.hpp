#pragma once

#include <type_traits>

namespace utils {

/**
 * Calculate ceil(n/d)
 */
template <typename T> inline constexpr T div_ceil(T n, T d) {
    static_assert(std::is_integral<T>::value, "Integral value required");
    if (n >= 0) {
        return (n + d - 1) / d;
    } else {
        return n / d;
    }
}

/**
 * Calculate floor(n/d)
 */
template <typename T> inline constexpr T div_floor(T n, T d) {
    static_assert(std::is_integral<T>::value, "Integral value required");
    if (n >= 0) {
        return n / d;
    } else {
        return (n - d + 1) / d;
    }
}

/**
 * Round up to multiple of m, where multiple can be negative
 */
template <typename T> inline constexpr T round_up(T v, T m) {
    return m * div_ceil(v, m);
}

/**
 * Round down to multiple of m, where multiple can be negative
 */
template <typename T> inline constexpr T round_down(T v, T m) {
    return m * div_floor(v, m);
}

/**
 * Calculate the absolute difference between two values
 */
template <typename T> constexpr T diff(T v1, T v2) {
    if (v1 > v2) {
        return v1 - v2;
    } else {
        return v2 - v1;
    }
}
} // namespace utils
