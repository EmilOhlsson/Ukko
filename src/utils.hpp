#pragma once

#include <cstdint>
#include <fmt/core.h>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <type_traits>

namespace utils {

/**
 * Calculate ceil(n/d)
 */
template <typename T> inline constexpr T div_ceil(T nom, T div) {
    static_assert(std::is_integral<T>::value, "Integral value required");
    if (nom >= 0) {
        return (nom + div - 1) / div;
    } else {
        return nom / div;
    }
}

/**
 * Calculate floor(n/d)
 */
template <typename T> inline constexpr T div_floor(T nom, T div) {
    static_assert(std::is_integral<T>::value, "Integral value required");
    if (nom >= 0) {
        return nom / div;
    } else {
        return (nom - div + 1) / div;
    }
}

/**
 * Round up to multiple of m, where multiple can be negative
 */
template <typename T> inline constexpr T round_up(T val, T mul) {
    return mul * div_ceil(val, mul);
}

/**
 * Round down to multiple of m, where multiple can be negative
 */
template <typename T> inline constexpr T round_down(T val, T mul) {
    return mul * div_floor(val, mul);
}

/**
 * Calculate the absolute difference between two values
 */
template <typename T> constexpr T diff(T val1, T val2) {
    if (val1 > val2) {
        return val1 - val2;
    } else {
        return val2 - val1;
    }
}

/**
 * Give a string view of a data span
 */
template <typename T> auto as_string_view(std::span<T> data) -> std::string_view {
    // NOLINTBEGIN
    return {reinterpret_cast<char *>(data.data()), data.size() * sizeof(T)};
    // NOLINTEND
}

template <typename In, typename Out> inline auto ptr_as_optional(In *input) -> std::optional<Out> {
    if (input != nullptr) {
        return input;
    }
    return std::nullopt;
}

inline constexpr ssize_t default_amount_of_bytes{512};

template <typename T>
inline auto dump_bytes(std::span<T> data, std::ostream &ostream = std::cout,
                       ssize_t nbr_of_bytes = default_amount_of_bytes) -> void {
    static constexpr size_t width{16};
    auto const bytes{std::as_bytes(data)};
    const size_t padding{width - bytes.size() % width};
    const size_t nbytes{std::min<size_t>(nbr_of_bytes, data.size())};
    for (size_t line{0}; line < nbytes; line += width) {
        for (size_t i{line}; i < std::min(nbytes, line + width); i += 1) {
            uint32_t byte = static_cast<uint32_t>(bytes[i]);
            ostream << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(byte)
                    << ' ';
        }

        if (size_t remaining = nbytes - line; remaining < width) {
            for (size_t i{0}; i < padding; i += 1) {
                ostream << "   ";
            }
        }

        ostream << '\t';
        for (size_t i{line}; i < std::min(nbytes, line + width); i += 1) {
            char byte = static_cast<char>(bytes[i]);
            ostream << (isgraph(byte) ? byte : '.');
        }
        ostream << '\n';
    }
}

template <typename T>
inline auto dump_bytes_as_string(std::span<T> data, ssize_t nbr_of_bytes = default_amount_of_bytes)
    -> std::string {
    std::stringstream stringstream{};
    dump_bytes(data, stringstream, nbr_of_bytes);
    return stringstream.str();
}

/**
 * Create an exception, but with libfmt syntax
 */
struct runtime_error : public std::runtime_error {
    template <typename S, typename... Args>
    runtime_error(const S &format, Args &&...args)
        : std::runtime_error{fmt::vformat(format, fmt::make_format_args(args...))} {
    }
};

struct String : public std::string {
    template <typename S, typename... Args>
    String(const S &format, Args &&...args)
        : std::string(fmt::vformat(format, fmt::make_format_args(args...))) {
    }
};

} // namespace utils
