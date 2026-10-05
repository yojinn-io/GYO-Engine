#pragma once

// The product's sampling for its math-foundation characterization tests. The
// bit helpers (Opaque, SameBits, UlpDistance) are the common ones in
// tests/common/support/CharacterizationBits.hpp.

#include <array>
#include <cstdint>
#include <limits>

namespace CharacterizationSupport {

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
