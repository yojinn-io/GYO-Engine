#pragma once

// The pose of an entity (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 4.1; Design
// Invariants in docs/architecture/target/v2/transform.zh-Hant.md). Transform is the author's, authoritative pose:
// relative to the parent, equal to the global pose without a parent. GlobalTransform is derived from it by the
// transform Module only and is for drawing: decisions never read it (docs/cross-component-rules.md).
// 2D games use the same types; the translation's z orders what is drawn in front.

#include <bit>
#include <cstdint>

#include "engine/kernel/Hash.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace GYO::Engine::Transform {

struct Transform final {
    Math::Vec3 translation{};
    Math::Quaternion rotation{};
    Math::Vec3 scale{1.0F, 1.0F, 1.0F};

    // Bit by bit (each float compares its bits: -0 and +0 differ, a NaN equals the same NaN bits), the same
    // equality HashValue hashes. A rollback registration uses it, so Restore writes exactly the cells whose hash
    // differs (rollback R-2).
    friend bool operator==(const Transform& a, const Transform& b) noexcept;
};

// The global matrix (Math conventions: column vectors, translation in values[12..14]).
struct GlobalTransform final {
    Math::Matrix4 matrix{};
};

[[nodiscard]] inline Math::Matrix4 ToMatrix(const Transform& transform) noexcept {
    return Math::ComposeTRS(transform.translation, transform.rotation, transform.scale);
}

namespace Detail {

[[nodiscard]] inline bool SameBits(const float a, const float b) noexcept {
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

} // namespace Detail

inline bool operator==(const Transform& a, const Transform& b) noexcept {
    using Detail::SameBits;
    return SameBits(a.translation.x, b.translation.x) && SameBits(a.translation.y, b.translation.y) &&
           SameBits(a.translation.z, b.translation.z) && SameBits(a.rotation.x, b.rotation.x) &&
           SameBits(a.rotation.y, b.rotation.y) && SameBits(a.rotation.z, b.rotation.z) &&
           SameBits(a.rotation.w, b.rotation.w) && SameBits(a.scale.x, b.scale.x) && SameBits(a.scale.y, b.scale.y) &&
           SameBits(a.scale.z, b.scale.z);
}

// Field by field: the author's state.
inline void HashValue(Kernel::Hasher& h, const Transform& v) {
    Kernel::HashValue(h, v.translation.x);
    Kernel::HashValue(h, v.translation.y);
    Kernel::HashValue(h, v.translation.z);
    Kernel::HashValue(h, v.rotation.x);
    Kernel::HashValue(h, v.rotation.y);
    Kernel::HashValue(h, v.rotation.z);
    Kernel::HashValue(h, v.rotation.w);
    Kernel::HashValue(h, v.scale.x);
    Kernel::HashValue(h, v.scale.y);
    Kernel::HashValue(h, v.scale.z);
}

// Hashes nothing: a derived value, propagated once a frame, not state two runs must agree on (TF-2).
inline void HashValue(Kernel::Hasher&, const GlobalTransform&) {}

} // namespace GYO::Engine::Transform
