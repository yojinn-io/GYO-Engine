#pragma once

// The collider component (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section 4.5): a shape on an
// entity, placed by the entity's Transform (never GlobalTransform: CCR-12, P3-2). Registered by Physics3dModule as
// "Physics3d.Collider"; registering it for rollback or the digest is the author's (as with Transform).

#include <bit>
#include <cstdint>
#include <type_traits>
#include <variant>

#include "engine/kernel/Hash.hpp"
#include "engine/physics_3d/Shapes.hpp"

namespace GYO::Engine::Physics3d {

struct Collider final {
    Shape shape{};

    // Bit by bit (each float compares its bits: -0 and +0 differ); a BoxSet compares as BoxSet's ==. The same
    // equality HashValue hashes (CCR-13), so a rollback restore writes exactly the cells whose hash differs.
    friend bool operator==(const Collider& a, const Collider& b) noexcept;
};

namespace Detail {

[[nodiscard]] inline bool SameBits(const float a, const float b) noexcept {
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

[[nodiscard]] inline bool SameShape(const Sphere& a, const Sphere& b) noexcept { return SameBits(a.radius, b.radius); }
[[nodiscard]] inline bool SameShape(const Capsule& a, const Capsule& b) noexcept {
    return SameBits(a.halfHeight, b.halfHeight) && SameBits(a.radius, b.radius);
}
[[nodiscard]] inline bool SameShape(const Cuboid& a, const Cuboid& b) noexcept {
    return SameBits(a.halfExtents.x, b.halfExtents.x) && SameBits(a.halfExtents.y, b.halfExtents.y) &&
           SameBits(a.halfExtents.z, b.halfExtents.z);
}
[[nodiscard]] inline bool SameShape(const BoxSet& a, const BoxSet& b) noexcept { return a == b; }

inline void HashShape(Kernel::Hasher& h, const Sphere& v) { Kernel::HashValue(h, v.radius); }
inline void HashShape(Kernel::Hasher& h, const Capsule& v) {
    Kernel::HashValue(h, v.halfHeight);
    Kernel::HashValue(h, v.radius);
}
inline void HashShape(Kernel::Hasher& h, const Cuboid& v) {
    Kernel::HashValue(h, v.halfExtents.x);
    Kernel::HashValue(h, v.halfExtents.y);
    Kernel::HashValue(h, v.halfExtents.z);
}
// The count and the digest Build computed: hashing tens of thousands of boxes every summary is not needed.
inline void HashShape(Kernel::Hasher& h, const BoxSet& v) {
    Kernel::HashValue(h, v.Size());
    Kernel::HashValue(h, v.Digest());
}

} // namespace Detail

inline bool operator==(const Collider& a, const Collider& b) noexcept {
    if (a.shape.index() != b.shape.index()) return false;
    return std::visit(
        [&b](const auto& shape) {
            using S = std::remove_cvref_t<decltype(shape)>;
            return Detail::SameShape(shape, *std::get_if<S>(&b.shape));
        },
        a.shape);
}

// Which shape, then its fields.
inline void HashValue(Kernel::Hasher& h, const Collider& v) {
    Kernel::HashValue(h, static_cast<std::uint64_t>(v.shape.index()));
    std::visit([&h](const auto& shape) { Detail::HashShape(h, shape); }, v.shape);
}

} // namespace GYO::Engine::Physics3d
