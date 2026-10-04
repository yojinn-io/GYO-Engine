#pragma once

// Helpers shared by the math-foundation characterization tests: run-time
// opaque inputs, bit comparison and distances in representable values.

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace CharacterizationSupport {

// Reading through a volatile keeps the compiler from constant-folding either
// side with a rounding that differs from run time.
[[nodiscard]] inline float Opaque(const float value) {
    volatile float stored = value;
    return stored;
}

[[nodiscard]] inline double Opaque(const double value) {
    volatile double stored = value;
    return stored;
}

// NaN equals NaN regardless of payload.
[[nodiscard]] inline bool SameBits(const float a, const float b) {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

[[nodiscard]] inline bool SameBits(const double a, const double b) {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// Distance in representable floats; -0 and +0 are 0 apart. NaN against a
// number is the largest distance.
[[nodiscard]] inline std::int64_t UlpDistance(const float a, const float b) {
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
[[nodiscard]] inline std::uint64_t UlpDistance(const double a, const double b) {
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

// SplitMix64 seeded through a volatile, so every sample is run-time data.
class Random final {
public:
    explicit Random(const std::uint64_t seed) {
        volatile std::uint64_t stored = seed;
        state_ = stored;
    }

    [[nodiscard]] std::uint64_t Next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    // Exact: a 24-bit integer scaled by a power of two.
    [[nodiscard]] float Unit() { return static_cast<float>(Next() >> 40U) * 0x1p-24F; }
    [[nodiscard]] float Range(const float lo, const float hi) { return lo + (hi - lo) * Unit(); }

    // Mostly ordinary values, sometimes a special one.
    [[nodiscard]] float Any(const float scale) {
        static constexpr std::array<float, 10> special{0.0F, -0.0F, 1.0F, -1.0F, 1.0e-40F, -1.0e-40F,
                                                      std::numeric_limits<float>::infinity(),
                                                      -std::numeric_limits<float>::infinity(),
                                                      std::numeric_limits<float>::quiet_NaN(), 3.0e38F};
        if (Next() % 16U == 0U) return special[Next() % special.size()];
        return Range(-scale, scale);
    }

private:
    std::uint64_t state_{};
};

} // namespace CharacterizationSupport
