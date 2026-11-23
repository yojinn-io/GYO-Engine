#include "engine/physics_3d/MoveAndSlide.hpp"

// avian src/character_controller/velocity_project.rs (project_velocity_bruteforce) and move_and_slide.rs
// (pull_back, depenetrate_intersections), in double with float at the interface.

#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace GYO::Engine::Physics3d {

namespace {

using Math::Vec3d;

bool IsUnitNormal(const Math::Vec3& n) {
    // Unit within float rounding: the planes come from contacts or from the author (normalized).
    const double squared = Math::Dot(Math::ToVec3d(n), Math::ToVec3d(n));
    return Math::IsFinite(n) && squared > 0.99 && squared < 1.01;
}

} // namespace

Math::Vec3 ProjectVelocity(const Math::Vec3 v, const std::span<const Math::Vec3> normals) {
    GYO_ASSERT(Math::IsFinite(v));
    if (normals.empty()) return v;
    const Vec3d velocity = Math::ToVec3d(v);
    const double epsilon = -static_cast<double>(kDotEpsilon);
    const auto valid = [&normals, epsilon](const Vec3d& projection) {
        for (const Math::Vec3& n : normals) {
            if (!(Math::Dot(projection, Math::ToVec3d(n)) >= epsilon)) return false;
        }
        return true;
    };
    // Inside the cone of half-spaces: unchanged.
    if (valid(velocity)) return v;
    Vec3d best{};
    double bestDistance = std::numeric_limits<double>::infinity();
    // One active plane: v - (v . n) n.
    for (const Math::Vec3& normal : normals) {
        const Vec3d n = Math::ToVec3d(normal);
        const double into = Math::Dot(n, velocity);
        if (into < epsilon) {
            const Vec3d projection = velocity - n * into;
            const double distance = Math::LengthSquared(velocity - projection);
            if (distance < bestDistance && valid(projection)) {
                bestDistance = distance;
                best = projection;
            }
        }
    }
    // Two active planes: v onto their edge e = ni x nj.
    for (std::size_t i = 0; i < normals.size(); ++i) {
        for (std::size_t j = i + 1; j < normals.size(); ++j) {
            const Vec3d edge = Math::Cross(Math::ToVec3d(normals[i]), Math::ToVec3d(normals[j]));
            const double edgeSquared = Math::LengthSquared(edge);
            if (edgeSquared < static_cast<double>(kDotEpsilon)) continue;  // nearly parallel
            const Vec3d projection = edge * (Math::Dot(velocity, edge) / edgeSquared);
            const double distance = Math::LengthSquared(velocity - projection);
            if (distance < bestDistance && valid(projection)) {
                bestDistance = distance;
                best = projection;
            }
        }
    }
    // No valid projection: the apex of the cone.
    return std::isinf(bestDistance) ? Math::Vec3{} : Math::ToVec3(best);
}

Math::Vec3 DepenetrateIntersections(const DepenetrationConfig& config,
                                    const std::span<const DepenetrationContact> contacts) {
    GYO_ASSERT(IsValid(config));
    Vec3d fixup{};
    const double rejection = config.penetrationRejectionThreshold;
    for (std::uint32_t pass = 0; pass < config.iterations; ++pass) {
        double total = 0.0;
        for (const DepenetrationContact& contact : contacts) {
            const double push = contact.push;
            if (push > rejection) continue;  // too deep to trust: left alone
            const Vec3d n = Math::ToVec3d(contact.normal);
            const double remaining = push - Math::Dot(fixup, n);
            const double error = remaining > 0.0 ? remaining : 0.0;
            total += error;
            fixup = fixup + n * error;
        }
        if (total < static_cast<double>(config.maxDepenetrationError)) break;
    }
    return Math::ToVec3(fixup);
}

bool IsValid(const DepenetrationConfig& config) noexcept {
    return std::isfinite(config.maxDepenetrationError) && config.maxDepenetrationError >= 0.0F &&
           std::isfinite(config.penetrationRejectionThreshold) && config.penetrationRejectionThreshold >= 0.0F &&
           std::isfinite(config.skinWidth) && config.skinWidth >= 0.0F;
}

bool IsValid(const MoveAndSlideConfig& config) noexcept {
    if (!IsValid(config.depenetration)) return false;
    if (!std::isfinite(config.planeSimilarityDotThreshold)) return false;
    for (const Math::Vec3& plane : config.planes) {
        if (!IsUnitNormal(plane)) return false;
    }
    return true;
}

Base::Result<std::optional<MoveHit>, Physics3dError> CastMoveWith(const Math::Vec3& movement, const float skinWidth,
                                                                 const MoveCast& cast) {
    GYO_ASSERT(Math::IsFinite(movement));
    GYO_ASSERT(std::isfinite(skinWidth) && skinWidth >= 0.0F);
    const Vec3d move = Math::ToVec3d(movement);
    const double length = Math::Length(move);
    const Vec3d direction = length > 0.0 ? move / length : Vec3d{1.0, 0.0, 0.0};
    const double castLength = length + static_cast<double>(skinWidth);
    ShapeCastOptions options;
    options.stopAtPenetration = false;  // avian's ignore_origin_penetration
    auto found = cast(Math::ToVec3(direction * castLength), options);
    if (!found) return Base::Err(std::move(found).error());
    if (!*found) return std::optional<MoveHit>{};
    const EntityShapeHit& hit = **found;
    const double hitDistance = static_cast<double>(hit.hit.time) * castLength;
    // pull_back: keep skinWidth from the plane, measured along its normal.
    double safe = 0.0;
    if (castLength != 0.0) {
        const double facing = -Math::Dot(direction, Math::ToVec3d(hit.hit.normal2));
        const double dot = facing > static_cast<double>(kDotEpsilon) ? facing : static_cast<double>(kDotEpsilon);
        const double skinDistance = static_cast<double>(skinWidth) / dot;
        const double back = hitDistance - skinDistance;
        safe = back > 0.0 ? back : 0.0;
    }
    MoveHit out;
    out.entity = hit.entity;
    out.distance = static_cast<float>(safe);
    out.collisionDistance = static_cast<float>(hitDistance);
    out.point = hit.hit.witness2;
    out.normal = hit.hit.normal2;
    out.shapePoint = hit.hit.witness1;
    return std::optional<MoveHit>(out);
}

} // namespace GYO::Engine::Physics3d
