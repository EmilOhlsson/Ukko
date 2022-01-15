#pragma once

namespace utils {

    /**
     * Calculate ceil(n/d)
     */
    template <typename T> inline constexpr T div_ceil(T n, T d) {
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
}
