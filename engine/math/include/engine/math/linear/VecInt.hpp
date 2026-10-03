#pragma once

#include <cstdint>
#include <type_traits>

namespace Engine::Math {

// Integer vectors for grid cells, pixel coordinates and counts. Exact, so
// equality is provided (float vectors deliberately have none).
struct Vec2i final {
    std::int32_t x{};
    std::int32_t y{};
    friend bool operator==(const Vec2i&, const Vec2i&) = default;
};

struct Vec3i final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
    friend bool operator==(const Vec3i&, const Vec3i&) = default;
};

static_assert(sizeof(Vec2i) == 8 && alignof(Vec2i) == 4 && sizeof(Vec3i) == 12 && alignof(Vec3i) == 4);
static_assert(std::is_trivially_copyable_v<Vec2i> && std::is_standard_layout_v<Vec2i>);
static_assert(std::is_trivially_copyable_v<Vec3i> && std::is_standard_layout_v<Vec3i>);
static_assert(std::is_aggregate_v<Vec2i> && std::is_aggregate_v<Vec3i>);

[[nodiscard]] constexpr Vec2i operator+(const Vec2i a, const Vec2i b) noexcept { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] constexpr Vec2i operator-(const Vec2i a, const Vec2i b) noexcept { return {a.x - b.x, a.y - b.y}; }
[[nodiscard]] constexpr Vec2i operator-(const Vec2i a) noexcept { return {-a.x, -a.y}; }
[[nodiscard]] constexpr Vec3i operator+(const Vec3i a, const Vec3i b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
[[nodiscard]] constexpr Vec3i operator-(const Vec3i a, const Vec3i b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
[[nodiscard]] constexpr Vec3i operator-(const Vec3i a) noexcept { return {-a.x, -a.y, -a.z}; }

} // namespace Engine::Math
