#pragma once

// Presentation formulas that math-foundation batch B6b replaced with
// Engine::Math, frozen from 4bfa764 with the same expression shape, operand
// order and types (file:line in each comment). Only characterization tests
// use them.

#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/render/RenderTypes.hpp"

#include <cmath>
#include <numbers>

namespace PresentationLegacy {

// WeaponPresentationDefinition.cpp:73-84 (EvaluateWeaponMuzzleViewCameraPosition
// after the muzzle point is transformed into model space).
[[nodiscard]] inline Engine::Math::Vec3 MuzzleViewCameraPosition(
    const Engine::Math::Vec3 point, const Engine::Math::Vec3 idleAnchor,
    const Engine::Render::Transform3D& transform) {
    Engine::Math::Vec3 result{
        (point.x - idleAnchor.x) * transform.scale.x,
        (point.y - idleAnchor.y) * transform.scale.y,
        (point.z - idleAnchor.z) * transform.scale.z};
    const float cx = std::cos(transform.rotationRadians.x), sx = std::sin(transform.rotationRadians.x);
    result = {result.x, result.y * cx - result.z * sx, result.y * sx + result.z * cx};
    const float cy = std::cos(transform.rotationRadians.y), sy = std::sin(transform.rotationRadians.y);
    result = {result.x * cy + result.z * sy, result.y, -result.x * sy + result.z * cy};
    const float cz = std::cos(transform.rotationRadians.z), sz = std::sin(transform.rotationRadians.z);
    result = {result.x * cz - result.y * sz, result.x * sz + result.y * cz, result.z};
    return {result.x + transform.translation.x, result.y + transform.translation.y,
            result.z + transform.translation.z};
}

// PlayerPresentation.cpp:672-677 (Submit, weaponWorldPosition from the mount
// point in model space).
[[nodiscard]] inline Engine::Math::Vec3 WeaponWorldPosition(
    Engine::Math::Vec3 point, const Engine::Math::Vec3 anchor, const float scale,
    const Engine::Math::Vec3 position, const float yaw) {
    point = {(point.x - anchor.x) * scale, (point.y - anchor.y) * scale,
             (point.z - anchor.z) * scale};
    return {
        position.x + std::cos(yaw) * point.x + std::sin(yaw) * point.z,
        position.y + point.y,
        position.z - std::sin(yaw) * point.x + std::cos(yaw) * point.z};
}

// PlayerPresentation.cpp:168-175 (LoadPlayerPresentationDefinition, weapon
// mount rotation after validation).
[[nodiscard]] inline Engine::Math::Quaternion NormalizeMountRotation(Engine::Math::Quaternion q) {
    const double norm = static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
        static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w;
    const auto inverse = static_cast<float>(1 / std::sqrt(norm));
    return {q.x * inverse, q.y * inverse, q.z * inverse, q.w * inverse};
}

// PlayerPresentation.cpp:168-172: the rotation part of the mount validation.
[[nodiscard]] inline bool AcceptsMountRotation(const Engine::Math::Quaternion q) {
    const double norm = static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
        static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w;
    return std::isfinite(norm) && !(norm < 1e-12);
}

// SnapshotTimeline.hpp:121-122 and PlayerPresentation.cpp:232-234: the
// horizontal length of a double XZ difference.
[[nodiscard]] inline double HorizontalLength(const double dx, const double dz) { return std::hypot(dx, dz); }

// SnapshotTimeline.hpp:118-119.
[[nodiscard]] inline float LerpYaw(const float a, const float b, const float fraction) {
    return std::remainder(a + std::remainder(b - a,
        2 * std::numbers::pi_v<float>) * fraction, 2 * std::numbers::pi_v<float>);
}

// PvpApplication.cpp:810-811 (mouse look).
[[nodiscard]] inline float WrapMouseYaw(const float yaw) {
    return std::remainder(yaw, 2.0F * std::numbers::pi_v<float>);
}

// WeaponPresentationDefinition.cpp:141-142 and :153.
[[nodiscard]] inline float DegreesToRadians(const float degrees) {
    constexpr float radians = std::numbers::pi_v<float> / 180.0F;
    return degrees * radians;
}

} // namespace PresentationLegacy
