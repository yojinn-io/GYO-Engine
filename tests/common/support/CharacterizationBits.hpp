#pragma once

// Bit-level helpers shared by characterization tests in the engine, tools and
// products: run-time opaque inputs, bit comparison and distances in
// representable values. Header-only, standard library only.

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace Engine::Test {

// Reading through a volatile keeps the compiler from constant-folding either
// side with a rounding that differs from run time.
[[nodiscard]] inline float Opaque(const float value) noexcept {
    volatile float stored = value;
    return stored;
}

[[nodiscard]] inline double Opaque(const double value) noexcept {
    volatile double stored = value;
    return stored;
}

// NaN equals NaN regardless of payload.
[[nodiscard]] inline bool SameBits(const float a, const float b) noexcept {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

[[nodiscard]] inline bool SameBits(const double a, const double b) noexcept {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// Distance in representable floats; -0 and +0 are 0 apart. NaN against a
// number is the largest distance.
[[nodiscard]] inline std::int64_t UlpDistance(const float a, const float b) noexcept {
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b) ? 0 : (std::numeric_limits<std::int64_t>::max)();
    }
    const auto ordered = [](const float value) {
        const auto bits = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(value));
        return bits < 0 ? std::int64_t{(std::numeric_limits<std::int32_t>::min)()} - bits : bits;
    };
    return std::llabs(ordered(a) - ordered(b));
}

// The same for doubles; saturates instead of overflowing across the sign.
[[nodiscard]] inline std::uint64_t UlpDistance(const double a, const double b) noexcept {
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b) ? 0 : (std::numeric_limits<std::uint64_t>::max)();
    }
    const auto ordered = [](const double value) {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        constexpr std::uint64_t sign = std::uint64_t{1} << 63U;
        return (bits & sign) != 0 ? sign - (bits & ~sign) : sign + bits;
    };
    const auto x = ordered(a), y = ordered(b);
    return x > y ? x - y : y - x;
}

} // namespace Engine::Test
