#pragma once

// The pure rules of CheckVisibility (VisibilityModule.hpp), for the System and for tests.

#include <optional>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Frustum.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/render/Drawables.hpp"

namespace GYO::Engine::Visibility {

// The world box around a local box placed by world: its eight corners transformed, then their bounds (conservative).
[[nodiscard]] Math::Aabb WorldBox(const Math::Aabb& local, const Math::Matrix4& world) noexcept;

// Whether the range lets a camera at this distance see the entity: start of startMargin <= distance < end of
// endMargin (Bevy's is_visible_at_all).
[[nodiscard]] bool InRange(const Render::VisibilityRange& range, float distance) noexcept;

// One entity seen from one camera: the frustum test on its Aabb (none, or noFrustumCulling: passes) and the range
// test (none: passes), the range's distance measured from cameraPosition to the entity's position (world's
// translation) or, with useAabb and an Aabb, to the box's centre.
struct Candidate final {
    Math::Matrix4 world{};
    const Render::Aabb* aabb = nullptr;
    const Render::VisibilityRange* range = nullptr;
    bool noFrustumCulling = false;
};
[[nodiscard]] bool Sees(const Math::Frustum& frustum, const Math::Vec3& cameraPosition, const Candidate& candidate) noexcept;

} // namespace GYO::Engine::Visibility
