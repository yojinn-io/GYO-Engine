#pragma once

// The pure queries of the physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md,
// section 4.3). They touch no World and can be called anywhere (tools, tests, a product's hit shapes).
//
// Numeric policy (section 4.2, P3-5): computed in double, each result rounded to float once; only + - * /,
// std::sqrt, comparisons and abs (no transcendental function, no long double, no fused multiply-add). There is no
// tolerance: an author who wants a skin width uses ShapeCastOptions::targetDistance. Touching counts as a hit.
// All points and normals are in world space.
//
// The supported pairs (shape 1 -> shape 2); any other pair returns Unsupported (P3-4):
//   CastRay     -> Ball, Capsule, Cuboid, BoxSet, any pose
//   FindContact  Ball, Capsule -> Ball, Capsule, Cuboid, BoxSet, any pose
//   CastShape    Ball -> Cuboid, BoxSet, any pose
//   CastShape    Capsule -> Cuboid, BoxSet, when the capsule's segment (b - a) in the box's local frame has exactly
//                two zero components (an upright capsule against axis-aligned boxes), or its ends coincide (a ball)
// Moving a shape against a Ball or a Capsule is not implemented (the PvP inventory: characters are never swept
// against each other; placement uses FindContact).

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/physics_3d/Physics3dError.hpp"
#include "engine/physics_3d/Shapes.hpp"

namespace GYO::Engine::Physics3d {

// parry's RayIntersection. t is in units of |direction| (as Math::Intersect): a unit direction makes it the
// distance. normal is the outward normal of the surface hit; a ray starting inside the solid shape (or on its
// surface) has t = 0 and a zero normal.
struct RayHit final {
    float t = 0.0F;
    Math::Vec3 normal{};
};

// parry's Contact. point1 is on shape 1's surface, point2 on shape 2's; normal1 is shape 1's outward normal,
// pointing toward shape 2, and normal2 = -normal1. distance < 0 is the penetration depth. When the cores of the
// shapes coincide the normal is +X (parry's choice for coincident balls).
struct Contact final {
    Math::Vec3 point1{};
    Math::Vec3 point2{};
    Math::Vec3 normal1{};
    Math::Vec3 normal2{};
    float distance = 0.0F;
};

// A contact with one box of a BoxSet and that box's index.
struct BoxContact final {
    Contact contact{};
    std::uint32_t box = 0;
};

enum class ShapeHitStatus : std::uint8_t {
    // The moving shape reached the target.
    Converged = 1,
    // At time 0 the shapes already overlap or are within targetDistance.
    PenetratingOrWithinTargetDistance = 2,
};

// parry's ShapeCastHit. time in [0, maxTime] is the fraction of the motion at the first contact; witness1 is on
// shape 1's surface (not inflated by targetDistance) at that time, witness2 on shape 2's; normal1 is shape 1's
// outward normal toward shape 2, normal2 = -normal1.
struct ShapeHit final {
    float time = 0.0F;
    Math::Vec3 witness1{};
    Math::Vec3 witness2{};
    Math::Vec3 normal1{};
    Math::Vec3 normal2{};
    ShapeHitStatus status = ShapeHitStatus::Converged;
};

// parry's ShapeCastOptions. targetDistance inflates shape 1 (a skin width): the cast stops that far from shape 2.
// stopAtPenetration false: a pair that overlaps (or is within targetDistance) at time 0 while the motion separates
// from it or is tangential (Dot(motion, normal1) <= 0) is not a hit; the cast goes on, and since a pair of convex
// shapes cannot meet again, that pair returns no hit.
struct ShapeCastOptions final {
    float maxTime = 1.0F;
    float targetDistance = 0.0F;
    bool stopAtPenetration = true;
};

// Content checks of the query arguments (a pure query asserts them).
// Finite origin and direction, direction not zero.
[[nodiscard]] bool IsValid(const Math::Ray& ray) noexcept;
// maxTime finite and >= 0, targetDistance finite and >= 0.
[[nodiscard]] bool IsValid(const ShapeCastOptions& options) noexcept;

// The first t in [0, maxT] at which the ray meets the solid shape. maxT >= 0 (infinity allowed). A product's hit
// shapes that are not entities (a bone's capsule in world space) are a Capsule{a, b, radius} at the identity Pose.
[[nodiscard]] std::optional<RayHit> CastRay(const Shape& shape, const Pose& pose, const Math::Ray& ray, float maxT);

// The contact when the distance between the shapes is at most prediction (>= 0). Against a BoxSet: the deepest
// contact (the smallest distance, ties to the lowest index).
//
// When the core of shape 1 (its centre or segment) touches or enters a box, the contact leaves the box through one
// of its faces: the face that needs the smallest push of the core along the face normal (ties in the order -X,
// +X, -Y, +Y, -Z, +Z); point2 is on that face and distance is -(push + radius).
[[nodiscard]] Base::Result<std::optional<Contact>, Physics3dError> FindContact(const Shape& shape1, const Pose& pose1,
                                                                               const Shape& shape2, const Pose& pose2,
                                                                               float prediction);
// Against a BoxSet: the deepest contact and its box, or every contact by increasing box index.
[[nodiscard]] Base::Result<std::optional<BoxContact>, Physics3dError> FindDeepestBoxContact(
    const Shape& shape1, const Pose& pose1, const BoxSet& boxes, const Pose& pose2, float prediction);
[[nodiscard]] Base::Result<std::vector<BoxContact>, Physics3dError> FindBoxContacts(
    const Shape& shape1, const Pose& pose1, const BoxSet& boxes, const Pose& pose2, float prediction);

// Shape 1 moves by motion (world space) from pose1; shape 2 stays at pose2. The first contact with time in
// [0, maxTime]; against a BoxSet the earliest (ties to the lowest index). A zero motion is an overlap test: a hit
// at time 0 when the pair overlaps or is within targetDistance and stopAtPenetration is true.
[[nodiscard]] Base::Result<std::optional<ShapeHit>, Physics3dError> CastShape(const Shape& shape1, const Pose& pose1,
                                                                              const Math::Vec3& motion,
                                                                              const Shape& shape2, const Pose& pose2,
                                                                              const ShapeCastOptions& options = {});

// The single-box cast of the BoxSet contract (P3-3) for box `box` of the set: shape 1 against that box alone, with
// the box's build data (its flush neighbours under FixInternalEdges), as CastShape against the set tests each box.
// box < boxes.Size().
[[nodiscard]] Base::Result<std::optional<ShapeHit>, Physics3dError> CastShapeOnBox(const Shape& shape1,
                                                                                   const Pose& pose1,
                                                                                   const Math::Vec3& motion,
                                                                                   const BoxSet& boxes, std::uint32_t box,
                                                                                   const Pose& pose2,
                                                                                   const ShapeCastOptions& options = {});

} // namespace GYO::Engine::Physics3d
