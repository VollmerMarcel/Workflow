#pragma once

#include <algorithm>
#include <cmath>

namespace carwear {

template <typename T>
constexpr T clamp(T value, T lo, T hi) noexcept {
    return value < lo ? lo : (value > hi ? hi : value);
}

inline double safe_divide(double numerator, double denominator, double fallback = 0.0) noexcept {
    return denominator == 0.0 ? fallback : numerator / denominator;
}

inline double square(double value) noexcept { return value * value; }

inline double indicator(bool condition) noexcept { return condition ? 1.0 : 0.0; }

}  // namespace carwear
