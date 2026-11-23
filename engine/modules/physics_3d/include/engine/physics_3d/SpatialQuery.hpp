#pragma once

// The spatial queries over a World's colliders (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md,
// section 4.5). avian's SpatialQuery system parameter; the kernel has no custom system parameters, so these are
// functions that take the author's own Query:
//
//     Kernel::Query<Kernel::Read<Physics3d::Collider>, Kernel::Read<Transform::Transform>,
//                   Kernel::Without<Kernel::ChildOf>> colliders
//
// The Query names Collider and Transform (Read or Write terms) and must carry Without<Kernel::ChildOf> (checked at
// compile time): a collider's pose is its Transform (CCR-12, P3-2; never GlobalTransform), which is the global pose
// only on an entity without a parent. Composing poses through ChildOf is a later batch (section 7). The Query may
// carry more terms (With<Wall>, ...) to narrow it.
//
// The entities are visited in ascending EntityId order (Query::Entities); the nearest result wins and a tie goes to
// the smaller EntityId. filter is a bool(EntityId) callable (avian's *_predicate); an entity it rejects is not
// looked at at all. A visited collider whose Transform's scale is not exactly (1, 1, 1) bit by bit makes the query
// return UnsupportedScale; a visited pair the pure query does not support makes it return Unsupported (P3-4).

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Physics3dError.hpp"
#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/transform/Transform.hpp"

namespace GYO::Engine::Physics3d {

// The default filter: every entity.
struct AcceptAll final {
    [[nodiscard]] constexpr bool operator()(Kernel::EntityId) const noexcept { return true; }
};

struct EntityRayHit final {
    Kernel::EntityId entity = 0;
    RayHit hit{};
};

struct EntityShapeHit final {
    Kernel::EntityId entity = 0;
    ShapeHit hit{};
};

namespace Detail {

template <class... Terms>
inline constexpr bool kWithoutChildOf = (std::is_same_v<Terms, Kernel::Without<Kernel::ChildOf>> || ...);

template <class... Terms>
constexpr void RequireColliderQuery() {
    static_assert(kWithoutChildOf<Terms...>,
                  "GYO_PHYSICS3D_REQUIRE_WITHOUT_CHILD_OF: the Query of a spatial query carries "
                  "Kernel::Without<Kernel::ChildOf>: a collider's pose is its Transform only without a parent");
}

[[nodiscard]] inline bool IsUnitScale(const Math::Vec3& scale) noexcept {
    constexpr std::uint32_t one = 0x3F800000U;
    return std::bit_cast<std::uint32_t>(scale.x) == one && std::bit_cast<std::uint32_t>(scale.y) == one &&
           std::bit_cast<std::uint32_t>(scale.z) == one;
}

[[nodiscard]] inline Physics3dError ScaleError(const Kernel::EntityId entity) {
    return Physics3dError::Make(Physics3dErrorCode::UnsupportedScale,
                                "a collider's Transform has a scale other than (1, 1, 1)",
                                "entity " + std::to_string(entity));
}

[[nodiscard]] inline Physics3dError AtEntity(Physics3dError error, const Kernel::EntityId entity) {
    error.detail = error.detail.empty() ? "entity " + std::to_string(entity)
                                        : error.detail + "; entity " + std::to_string(entity);
    return error;
}

// Calls visit(entity, collider, pose) for every entity of the Query the filter accepts, in ascending EntityId
// order; stops at the first error (a scale or what visit returns).
template <class... Terms, class Filter, class Visit>
[[nodiscard]] Base::Result<void, Physics3dError> ForEachCollider(const Kernel::Query<Terms...>& colliders,
                                                                 Filter& filter, Visit&& visit) {
    for (const Kernel::EntityId entity : colliders.Entities()) {
        if (!filter(entity)) continue;
        const Collider* collider = colliders.template Get<Collider>(entity);
        const Transform::Transform* transform = colliders.template Get<Transform::Transform>(entity);
        if (!IsUnitScale(transform->scale)) return Base::Err(ScaleError(entity));
        auto visited = visit(entity, *collider, Pose{transform->translation, transform->rotation});
        if (!visited) return Base::Err(AtEntity(std::move(visited).error(), entity));
    }
    return {};
}

} // namespace Detail

// The nearest collider the ray meets with t in [0, maxT] (RayHit's units).
template <class... Terms, class Filter = AcceptAll>
[[nodiscard]] Base::Result<std::optional<EntityRayHit>, Physics3dError> CastRay(
    const Kernel::Query<Terms...>& colliders, const Math::Ray& ray, const float maxT, Filter filter = {}) {
    Detail::RequireColliderQuery<Terms...>();
    std::optional<EntityRayHit> best;
    auto done = Detail::ForEachCollider(
        colliders, filter,
        [&](const Kernel::EntityId entity, const Collider& collider,
            const Pose& pose) -> Base::Result<void, Physics3dError> {
            const std::optional<RayHit> hit = CastRay(collider.shape, pose, ray, maxT);
            if (hit && (!best || hit->t < best->hit.t)) best = EntityRayHit{entity, *hit};
            return {};
        });
    if (!done) return Base::Err(std::move(done).error());
    return best;
}

// The earliest collider shape (at pose, moving by motion) meets; the pair table of CastShape (Queries.hpp).
template <class... Terms, class Filter = AcceptAll>
[[nodiscard]] Base::Result<std::optional<EntityShapeHit>, Physics3dError> CastShape(
    const Kernel::Query<Terms...>& colliders, const Shape& shape, const Pose& pose, const Math::Vec3& motion,
    const ShapeCastOptions& options, Filter filter = {}) {
    Detail::RequireColliderQuery<Terms...>();
    std::optional<EntityShapeHit> best;
    auto done = Detail::ForEachCollider(
        colliders, filter,
        [&](const Kernel::EntityId entity, const Collider& collider,
            const Pose& colliderPose) -> Base::Result<void, Physics3dError> {
            auto hit = CastShape(shape, pose, motion, collider.shape, colliderPose, options);
            if (!hit) return Base::Err(std::move(hit).error());
            if (*hit && (!best || (*hit)->time < best->hit.time)) best = EntityShapeHit{entity, **hit};
            return {};
        });
    if (!done) return Base::Err(std::move(done).error());
    return best;
}

// The colliders shape (at pose) overlaps or touches (FindContact with prediction 0), by ascending EntityId.
template <class... Terms, class Filter = AcceptAll>
[[nodiscard]] Base::Result<std::vector<Kernel::EntityId>, Physics3dError> Intersections(
    const Kernel::Query<Terms...>& colliders, const Shape& shape, const Pose& pose, Filter filter = {}) {
    Detail::RequireColliderQuery<Terms...>();
    std::vector<Kernel::EntityId> found;
    auto done = Detail::ForEachCollider(
        colliders, filter,
        [&](const Kernel::EntityId entity, const Collider& collider,
            const Pose& colliderPose) -> Base::Result<void, Physics3dError> {
            auto contact = FindContact(shape, pose, collider.shape, colliderPose, 0.0F);
            if (!contact) return Base::Err(std::move(contact).error());
            if (*contact) found.push_back(entity);
            return {};
        });
    if (!done) return Base::Err(std::move(done).error());
    return found;
}

} // namespace GYO::Engine::Physics3d
