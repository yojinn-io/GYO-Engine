#pragma once

// The kinematic character mechanism of the physics_3d Module: move and slide (avian's MoveAndSlide,
// src/character_controller/move_and_slide.rs, and velocity_project.rs; the primary reference, AGENTS §2.1).
// Functions over the author's Query and filter, like SpatialQuery.hpp (the kernel has no custom system parameters).
//
// Mechanism only (§1-1): moving a shape through the colliders and sliding along what it touches. The rules that
// vary between games stay the author's: whether a character is grounded (a downward CastMove or CastShape and the
// author's own normal threshold; a ground plane can be handed in through MoveAndSlideConfig::planes), slopes,
// steps, snapping to the ground, gravity. rapier's KinematicCharacterController bundles those; this does not.
//
// Units are the author's (metres in GYO's products); there is no length-unit scale (avian's PhysicsLengthUnit).
// Numeric policy as Queries.hpp: double inside, float at the interface, only + - * / sqrt and comparisons. The
// iteration is deterministic: entities by ascending EntityId, a BoxSet's boxes by ascending index.
// Query errors propagate: a pair the pure queries do not support (moving against a Ball or Capsule collider) is
// Unsupported, so a character's own and other characters' colliders are left out with the filter.

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/physics_3d/Physics3dError.hpp"
#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/physics_3d/SpatialQuery.hpp"

namespace GYO::Engine::Physics3d {

// avian's DOT_EPSILON: keeps a pull back finite when the motion grazes a surface, and is the tolerance of
// ProjectVelocity's half-space tests.
inline constexpr float kDotEpsilon = 0.005F;
// cos(5 degrees) as a literal (avian's COS_5_DEGREES): the default similarity of two contact planes.
inline constexpr float kCos5Degrees = 0.99619469F;
// avian's MIN_DISTANCE: a sweep shorter than this ends the loop.
inline constexpr float kMinMoveDistance = 1.0e-4F;

// avian's DepenetrationConfig, with its defaults.
struct DepenetrationConfig final {
    // 0 turns depenetration off.
    std::uint32_t iterations = 16;
    // Stop once the error a pass fixed is below this.
    float maxDepenetrationError = 1.0e-4F;
    // A contact deeper than this is not resolved (avian: it would push through geometry).
    float penetrationRejectionThreshold = 0.5F;
    // The distance always kept from the colliders.
    float skinWidth = 0.01F;
};

// avian's MoveAndSlideConfig, with its defaults (avian repeats the four depenetration fields; here they are one
// DepenetrationConfig, whose skinWidth is also the distance the move keeps).
struct MoveAndSlideConfig final {
    std::uint32_t moveAndSlideIterations = 4;
    DepenetrationConfig depenetration{};
    // Planes (unit normals) the move never goes against, before any contact: often the ground plane of a grounded
    // character (the author decides that it is grounded).
    std::vector<Math::Vec3> planes;
    // Two planes with Dot(n1, n2) >= this count as one; the more blocking one is kept.
    float planeSimilarityDotThreshold = kCos5Degrees;
    std::uint32_t maxPlanes = 20;
};

// avian's MoveHitData (CastMove's result). point is on the hit collider and normal is its outward normal (pointing
// at the moving shape: the plane the move slides along); shapePoint is on the moving shape.
struct MoveHit final {
    Kernel::EntityId entity = 0;
    // How far the shape can move along the movement and still keep skinWidth from the collider (0 when it starts
    // within skinWidth or overlapping).
    float distance = 0.0F;
    // The raw distance along the movement to the contact, ignoring skinWidth (0: overlapping at the start).
    float collisionDistance = 0.0F;
    Math::Vec3 point{};
    Math::Vec3 normal{};
    Math::Vec3 shapePoint{};

    [[nodiscard]] bool Intersects() const noexcept { return collisionDistance == 0.0F; }
};

struct MoveAndSlideOutput final {
    // Where the shape ends: the author writes it into Transform::translation.
    Math::Vec3 position{};
    // The velocity left after sliding along every plane (the next step's input velocity, if the author wants).
    Math::Vec3 projectedVelocity{};
};

enum class MoveAndSlideHitResponse : std::uint8_t {
    // Slide along this plane.
    Accept = 1,
    // Leave this plane out and go on.
    Ignore = 2,
    // Stop the loop (the final depenetration still runs).
    Abort = 3,
};

// avian's MoveAndSlideHitData. The callback may change normal (the plane used), position and velocity.
struct MoveAndSlideHitData final {
    Kernel::EntityId entity;
    // The sweep hit's distance and collisionDistance of this iteration (MoveHit).
    float distance;
    float collisionDistance;
    // On the hit collider.
    Math::Vec3 point;
    Math::Vec3& normal;
    Math::Vec3& position;
    Math::Vec3& velocity;

    [[nodiscard]] bool Intersects() const noexcept { return collisionDistance == 0.0F; }
};

// The default callback: every plane.
struct AcceptAllHits final {
    [[nodiscard]] MoveAndSlideHitResponse operator()(const MoveAndSlideHitData&) const noexcept {
        return MoveAndSlideHitResponse::Accept;
    }
};

// One contact for depenetration: the normal pointing from the collider at the shape, and how far to push along
// it (the penetration depth plus skinWidth).
struct DepenetrationContact final {
    Math::Vec3 normal{};
    float push = 0.0F;
};

// The velocity closest to v that does not point into any of the planes (unit normals): v when it is inside the
// cone of half-spaces (within kDotEpsilon), else the best valid projection onto one plane or onto the edge of two,
// else zero. avian's project_velocity_bruteforce, not its faster dual solver: the plane counts are small
// (maxPlanes 20), the brute force has no iteration or tolerance of its own to get wrong, and its result depends
// only on the order of the normals, which is fixed.
[[nodiscard]] Math::Vec3 ProjectVelocity(Math::Vec3 v, std::span<const Math::Vec3> normals);

// avian's depenetrate_intersections: Gauss-Seidel over the contacts, at most config.iterations passes, until a pass
// fixes less than maxDepenetrationError; a contact deeper than penetrationRejectionThreshold is skipped.
[[nodiscard]] Math::Vec3 DepenetrateIntersections(const DepenetrationConfig& config,
                                                  std::span<const DepenetrationContact> contacts);

[[nodiscard]] bool IsValid(const DepenetrationConfig& config) noexcept;
[[nodiscard]] bool IsValid(const MoveAndSlideConfig& config) noexcept;

// The shape cast CastMoveWith runs: motion and options in, the earliest hit (or an error) out.
using MoveCast = std::function<Base::Result<std::optional<EntityShapeHit>, Physics3dError>(
    const Math::Vec3& motion, const ShapeCastOptions& options)>;

// CastMove's arithmetic over a given cast (CastMove passes the World's CastShape): the cast motion (movement plus
// skinWidth along its direction; +X for a zero movement), avian's pull_back (skinWidth / max(Dot(direction,
// -normal), kDotEpsilon), so the shape stays skinWidth from the plane measured along its normal) and the MoveHit.
[[nodiscard]] Base::Result<std::optional<MoveHit>, Physics3dError> CastMoveWith(const Math::Vec3& movement,
                                                                                float skinWidth, const MoveCast& cast);

// avian's cast_move: casts shape from pose along movement, plus skinWidth (>= 0), and returns the first collider
// with the distance that keeps skinWidth from it. Overlaps at the start are ignored unless the movement goes into
// them (ShapeCastOptions::stopAtPenetration false). A zero movement casts skinWidth along +X (as avian).
template <class... Terms, class Filter = AcceptAll>
[[nodiscard]] Base::Result<std::optional<MoveHit>, Physics3dError> CastMove(const Kernel::Query<Terms...>& colliders,
                                                                            const Shape& shape, const Pose& pose,
                                                                            const Math::Vec3& movement,
                                                                            const float skinWidth, Filter filter = {}) {
    return CastMoveWith(movement, skinWidth, [&](const Math::Vec3& motion, const ShapeCastOptions& options) {
        return CastShape(colliders, shape, pose, motion, options, filter);
    });
}

// avian's depenetrate: the offset that pushes shape (at pose) out of every collider to skinWidth (contacts within
// skinWidth, one per collider and per BoxSet box).
template <class... Terms, class Filter = AcceptAll>
[[nodiscard]] Base::Result<Math::Vec3, Physics3dError> Depenetrate(const Kernel::Query<Terms...>& colliders,
                                                                   const Shape& shape, const Pose& pose,
                                                                   const DepenetrationConfig& config,
                                                                   Filter filter = {}) {
    GYO_ASSERT(IsValid(config));
    if (config.iterations == 0) return Math::Vec3{};
    std::vector<DepenetrationContact> contacts;
    auto visited = ForEachContact(colliders, shape, pose, config.skinWidth, filter,
                                  [&contacts, &config](Kernel::EntityId, std::uint32_t, const Contact& contact) {
                                      const double push = -static_cast<double>(contact.distance) +
                                                          static_cast<double>(config.skinWidth);
                                      contacts.push_back({contact.normal2, static_cast<float>(push)});
                                      return true;
                                  });
    if (!visited) return Base::Err(std::move(visited).error());
    return DepenetrateIntersections(config, contacts);
}

// avian's move_and_slide: depenetrate; then up to moveAndSlideIterations times sweep along velocity * time left,
// move up to the hit, collect the hit plane and the contact planes within 2 * skinWidth (nearly parallel planes
// merged, keeping the more blocking one; at most maxPlanes), and project the velocity onto them; depenetrate again.
// onHit is called for the sweep hit and for each new contact plane (MoveAndSlideHitResponse).
template <class... Terms, class Filter = AcceptAll, class OnHit = AcceptAllHits>
[[nodiscard]] Base::Result<MoveAndSlideOutput, Physics3dError> MoveAndSlide(
    const Kernel::Query<Terms...>& colliders, const Shape& shape, const Pose& pose, const Math::Vec3& velocity,
    const float deltaSeconds, const MoveAndSlideConfig& config, Filter filter = {}, OnHit onHit = {}) {
    GYO_ASSERT(Math::IsFinite(velocity));
    GYO_ASSERT(std::isfinite(deltaSeconds) && deltaSeconds >= 0.0F);
    GYO_ASSERT(IsValid(config));
    using Math::ToVec3;
    using Math::ToVec3d;
    const DepenetrationConfig& depenetration = config.depenetration;
    Math::Vec3 position = pose.translation;
    Math::Vec3 current = velocity;
    float timeLeft = deltaSeconds;
    const float skinWidth = depenetration.skinWidth;

    auto first = Depenetrate(colliders, shape, Pose{position, pose.rotation}, depenetration, filter);
    if (!first) return Base::Err(std::move(first).error());
    position = ToVec3(ToVec3d(position) + ToVec3d(*first));

    for (std::uint32_t iteration = 0; iteration < config.moveAndSlideIterations; ++iteration) {
        const Math::Vec3d sweep = ToVec3d(current) * static_cast<double>(timeLeft);
        const double distance = Math::Length(sweep);
        if (!(distance > 0.0)) break;  // no movement left (finite float inputs cannot overflow a double here)
        if (distance < static_cast<double>(kMinMoveDistance)) break;
        const Math::Vec3d direction = sweep / distance;

        auto cast = CastMove(colliders, shape, Pose{position, pose.rotation}, ToVec3(sweep), skinWidth, filter);
        if (!cast) return Base::Err(std::move(cast).error());
        if (!*cast) {
            position = ToVec3(ToVec3d(position) + sweep);  // nothing in the way: the whole sweep
            break;
        }
        const MoveHit hit = **cast;
        const double moved = static_cast<double>(hit.distance);
        timeLeft = static_cast<float>(static_cast<double>(timeLeft) -
                                      static_cast<double>(timeLeft) * (moved / distance));
        position = ToVec3(ToVec3d(position) + direction * moved);

        std::vector<Math::Vec3> planes = config.planes;
        Math::Vec3 firstNormal = hit.normal;
        const MoveAndSlideHitResponse response =
            onHit(MoveAndSlideHitData{hit.entity, hit.distance, hit.collisionDistance, hit.point, firstNormal, position,
                                      current});
        if (response == MoveAndSlideHitResponse::Accept) {
            planes.push_back(firstNormal);
        } else if (response == MoveAndSlideHitResponse::Abort) {
            break;
        }

        bool aborted = false;
        auto collected = ForEachContact(
            colliders, shape, Pose{position, pose.rotation}, skinWidth * 2.0F, filter,
            [&](const Kernel::EntityId entity, std::uint32_t, const Contact& contact) {
                Math::Vec3 normal = contact.normal2;
                const Math::Vec3d n = ToVec3d(normal);
                const Math::Vec3d v = ToVec3d(current);
                for (Math::Vec3& existing : planes) {
                    if (Math::Dot(n, ToVec3d(existing)) >= static_cast<double>(config.planeSimilarityDotThreshold)) {
                        // The same plane: keep the more blocking of the two.
                        if (Math::Dot(n, v) < Math::Dot(ToVec3d(existing), v)) existing = normal;
                        return true;
                    }
                }
                if (planes.size() >= config.maxPlanes) return false;
                const MoveAndSlideHitResponse answer = onHit(MoveAndSlideHitData{
                    entity, hit.distance, hit.collisionDistance, contact.point2, normal, position, current});
                if (answer == MoveAndSlideHitResponse::Accept) {
                    planes.push_back(normal);
                    return true;
                }
                if (answer == MoveAndSlideHitResponse::Ignore) return true;
                aborted = true;
                return false;
            });
        if (!collected) return Base::Err(std::move(collected).error());

        current = ProjectVelocity(current, planes);
        if (aborted) break;
    }

    auto last = Depenetrate(colliders, shape, Pose{position, pose.rotation}, depenetration, filter);
    if (!last) return Base::Err(std::move(last).error());
    position = ToVec3(ToVec3d(position) + ToVec3d(*last));
    return MoveAndSlideOutput{position, current};
}

} // namespace GYO::Engine::Physics3d
