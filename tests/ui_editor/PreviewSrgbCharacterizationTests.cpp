// Characterization of the ui_editor preview's private sRGB encoder against
// Engine::Math::EncodeSrgb (math-foundation plan, batch B5 prerequisite).
//
// Legacy::LinearToSrgb is frozen verbatim from
// tools/ui_editor/src/PreviewAdapter.cpp:27-32 at master 43bccad (same
// expression shape, operand order and types). B5 replaces that helper with
// Engine::Math::EncodeSrgb; this test proves the replacement is bit-identical
// on the running platform. NaN equals NaN regardless of payload.
//
// Every input is read back through a volatile so the compiler cannot
// constant-fold std::pow (a compile-time evaluation may round differently
// from the runtime libm that the editor actually uses).

#include "engine/math/scalar/ColorSpace.hpp"

#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace Legacy {

// tools/ui_editor/src/PreviewAdapter.cpp:27-32 at master 43bccad.
[[nodiscard]] float LinearToSrgb(const float linear) noexcept {
    const float clamped = std::clamp(linear, 0.0F, 1.0F);
    return clamped <= 0.0031308F
        ? clamped * 12.92F
        : 1.055F * std::pow(clamped, 1.0F / 2.4F) - 0.055F;
}

} // namespace Legacy

namespace {

constexpr float kInfinity = std::numeric_limits<float>::infinity();

// Read through a volatile at run time; see the file comment.
volatile std::uint32_t gSeed = 0x2545F491U;

[[nodiscard]] float Opaque(const float value) noexcept {
    volatile float stored = value;
    return stored;
}

// Fixed-seed linear congruential generator (Numerical Recipes constants).
class Lcg final {
public:
    explicit Lcg(const std::uint32_t seed) noexcept : state_(seed) {}

    std::uint32_t Next() noexcept {
        state_ = state_ * 1664525U + 1013904223U;
        return state_;
    }

    // Uniform in [lo, hi] using the top 24 bits.
    float Uniform(const float lo, const float hi) noexcept {
        const float unit = static_cast<float>(Next() >> 8U) * (1.0F / 16777216.0F);
        return lo + (hi - lo) * unit;
    }

private:
    std::uint32_t state_;
};

[[nodiscard]] bool SameBits(const float a, const float b) noexcept {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

[[nodiscard]] std::string Describe(const float value) {
    std::ostringstream stream;
    stream << std::hexfloat << value << " (0x" << std::hex << std::uppercase
           << std::setw(8) << std::setfill('0')
           << std::bit_cast<std::uint32_t>(value) << ')';
    return stream.str();
}

[[nodiscard]] std::vector<float> Sweep() {
    std::vector<float> sweep;
    // Every 8-bit channel value, in both spellings the engine uses.
    for (unsigned channel = 0; channel < 256U; ++channel) {
        sweep.push_back(static_cast<float>(channel) / 255.0F);
        sweep.push_back(static_cast<float>(channel) * (1.0F / 255.0F));
    }
    // Neighbours of the linear-segment threshold and of the clamp bounds.
    for (const float edge : {0.0031308F, 0.0F, 1.0F}) {
        float below = edge;
        float above = edge;
        sweep.push_back(edge);
        for (int step = 0; step < 4; ++step) {
            below = std::nextafter(below, -kInfinity);
            above = std::nextafter(above, kInfinity);
            sweep.push_back(below);
            sweep.push_back(above);
        }
    }
    // Pseudo-random values in [-1, 2] from a seed only known at run time.
    Lcg random{gSeed};
    for (int index = 0; index < 4096; ++index) {
        sweep.push_back(random.Uniform(-1.0F, 2.0F));
    }
    // Special values.
    for (const float special : {
             0.0F, -0.0F,
             std::numeric_limits<float>::denorm_min(),
             -std::numeric_limits<float>::denorm_min(),
             FLT_MIN, -FLT_MIN,
             0.5F, -0.5F, 1.0F, -1.0F, 2.0F,
             FLT_MAX, std::numeric_limits<float>::lowest(),
             kInfinity, -kInfinity,
             std::numeric_limits<float>::quiet_NaN(),
             -std::numeric_limits<float>::quiet_NaN(),
         }) {
        sweep.push_back(special);
    }
    return sweep;
}

} // namespace

int main() {
    const std::vector<float> sweep = Sweep();
    std::size_t mismatched = 0;
    for (std::size_t index = 0; index < sweep.size(); ++index) {
        const float input = Opaque(sweep[index]);
        const float legacy = Legacy::LinearToSrgb(input);
        const float math = Engine::Math::EncodeSrgb(Opaque(sweep[index]));
        if (SameBits(legacy, math)) continue;
        if (mismatched++ < 8) {
            std::cerr << "FAILED: case " << index << " input " << Describe(input)
                      << ": legacy " << Describe(legacy) << " math "
                      << Describe(math) << '\n';
        }
    }
    if (mismatched != 0) {
        std::cerr << mismatched << " of " << sweep.size()
                  << " preview sRGB encodes differ from Engine::Math::EncodeSrgb\n";
        return 1;
    }
    std::cout << "PreviewAdapter LinearToSrgb equals Engine::Math::EncodeSrgb on "
              << sweep.size() << " inputs\n";
    return 0;
}
